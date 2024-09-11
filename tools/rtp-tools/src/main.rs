use std::{cmp::min, fs::File, io::Read, vec::Vec};

use anyhow::Result;

use clap::{Parser, Subcommand};

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

fn main() -> Result<()> {
    let cli = Args::parse();

    let local_addr = match cli.bind {
        Some(s) => s,
        None => "0.0.0.0:6767".to_string(),
    };

    let pacing = std::time::Duration::new(0, 10000);

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

            let mut i = 0;
            for packet in packets {
                std::thread::sleep(pacing);
                print!("Playing chunk {i} of {packet_len}\r");
                udp_socket.send(&packet)?;
                i += 1;
            }

            println!("Finished playing all the things.");
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
