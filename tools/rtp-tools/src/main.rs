use std::{
    fs::File,
    io::Read,
    vec::Vec,
    cmp::min
};

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

    match &cli.command {
        Some(Commands::Play { file, addr }) => {
            println!("Playing {file:?} -> {addr}");
            //open file
            let mut fd = File::open(file)?;

            //buffer
            let mut file_buffer = Vec::with_capacity(1500 * 512);
            let read_bytes = fd.read_to_end(&mut file_buffer)?;

            let buffer_len = file_buffer.len();
            println!("ReadBytes {read_bytes} Len {buffer_len}");

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

            for packet in packets {
                let s = String::from_utf8(packet)?;
                println!("Packet: {s}");
            }

            //open socket

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
