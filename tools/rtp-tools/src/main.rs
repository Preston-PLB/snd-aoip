use std::{cmp::min, fs::File, io::Read, vec::Vec, time::Duration};

use anyhow::Result;

use clap::{Parser, Subcommand};
use rtp_rs::*;

#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
struct Args {
    #[arg(short, long, value_name = "x.x.x.x:port")]
    bind: Option<String>,

    #[command(subcommand)]
    command: Option<Commands>,
}

#[derive(Subcommand, Debug)]
enum Commands {
    Play {
        file: std::path::PathBuf,
        addr: String,
    },
    Listen,
}

const MTU: usize = 1500;

fn pacing(sample_rate: u32, bitdepth: usize, size: usize) -> Duration {
    let samples = size / bitdepth;
    let seconds = samples as f32 / sample_rate as f32;
    let just_seconds = seconds.floor();
    let nanos = (seconds - just_seconds) * 1e9;

    Duration::new(just_seconds as u64, nanos as u32)
}

fn main() -> Result<()> {
    let cli = Args::parse();

    let local_addr = match cli.bind {
        Some(s) => s,
        None => "0.0.0.0:6767".to_string(),
    };

    match &cli.command {
        Some(Commands::Play { file, addr }) => {
            println!("Playing {file:?} -> {addr}");
            //open file
            let mut fd = File::open(file)?;

            //read file into memory
            let mut file_buffer = Vec::with_capacity(1500 * 4096); //16 packets
            let buffer_len = fd.read_to_end(&mut file_buffer)?;

            //Construct the size of data allowed
            //TODO: Support variable MTU. Also calculate RTP packet size
            let header_len = MTU - 8 - 176;
            let mut packet_len = buffer_len / header_len;
            if buffer_len % header_len > 0 {
                packet_len += 1
            }

            let mut packets: Vec<Vec<u8>> = vec![Vec::with_capacity(header_len); packet_len];
            let loop_len = min(buffer_len, header_len);

            'outer: for i in 0..loop_len {
                for j in 0..packet_len {
                    let index = i + (header_len * j);
                    if index == buffer_len {
                        break 'outer;
                    }
                    packets[j].push(file_buffer[index]);
                }
            }

            //open socket
            let udp_socket = std::net::UdpSocket::bind(local_addr)?;
            udp_socket.connect(addr)?;

            let pacing = pacing(48000, 16, header_len);
            let mut i = 1;
            for packet in packets {
                print!("Playing chunk {i} of {packet_len}\r");
                let p = RtpPacketBuilder::new()
                    .payload_type(111)
                    .ssrc(1337)
                    .sequence(Seq::from(i))
                    .timestamp(10000+i as u32)
                    .marked(true)
                    .payload(&packet)
                    .build().unwrap(); //probably unsafe but its testing
                udp_socket.send(p.as_slice())?;
                i += 1;
                std::thread::sleep(pacing);
            }

            println!("\nFinished playing all the things.");
            //stream file to socket
            Ok(())
        }
        Some(Commands::Listen) => {
            println!("Listen!");
            Ok(())
        }
        None => {
            println!("Use --help for commands");
            Ok(())
        }
    }
}
