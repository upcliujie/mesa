use grtrace::parse::{Call, Data, Opcode, Vertex};
use std::fs::File;
use std::io::{BufReader, Read, Write};

macro_rules! dump {
    ($out:ident, $func:tt) => {{
        $out.write_all(&($func as u32).to_le_bytes()).unwrap();
    }};
    ($out:ident, $func:tt, $($arg:expr),*) => {{
        $out.write_all(&($func as u32).to_le_bytes()).unwrap();
        $(
            $out.write_all(&($arg as u32).to_le_bytes()).unwrap();
        )*
    }};
}

fn dump_vertex<W: Write>(mut out: W, vtx: &Vertex) {
    out.write_all(b"gVTX").unwrap();
    for float in vtx.0 {
        out.write_all(&float.to_le_bytes()).unwrap();
    }
}

fn dump_lfb<W: Write>(mut out: W, lfb: &[u16]) {
    out.write_all(b"gLFB").unwrap();
    let lfb = unsafe { std::slice::from_raw_parts(lfb.as_ptr() as *const u8, lfb.len() * 2) };
    out.write_all(lfb).unwrap();
}

fn main() {
    let args: Vec<_> = std::env::args().collect();
    if args.len() != 4 {
        eprintln!("Usage: {} <file.grtrace> <frame> <out.grtrace>", args[0]);
        std::process::exit(1);
    }

    let buf = {
        let filename = &args[1];
        let file = File::open(filename).unwrap();
        let mut reader = BufReader::new(file);
        let mut buf = Vec::new();
        reader.read_to_end(&mut buf).unwrap();
        buf
    };

    let max_frame: usize = args[2].parse().unwrap();

    let filename = &args[3];
    let mut out = File::create(filename).unwrap();
    out.write_all(b"grTR\0\0\0\0").unwrap();

    let mut num_frame = 0;

    let (mut i, _) = grtrace::parse::header(&buf).unwrap();
    loop {
        let (i2, call) = Call::parse(i).unwrap();
        i = i2;
        if let Some(call) = call {
            let args = call.args;
            match call.opcode {
                Opcode::GlideInit => dump!(out, 0),
                Opcode::GlideShutdown => dump!(out, 1),
                Opcode::SstQueryHardware => {
                    // TODO: remove this second parameter from the other dump tool.
                    dump!(out, 2, 0);
                }
                Opcode::SstSelect => dump!(out, 3, args[0]),
                Opcode::SstOpen => {
                    dump!(out, 4, args[0], args[1], args[2], args[3], args[4], args[5])
                }
                Opcode::SstPassthruMode => dump!(out, 5, args[0]),
                Opcode::BufferClear => dump!(out, 6, args[0], args[1], args[2]),
                Opcode::BufferSwap => {
                    // TODO: handle LFB stuff!
                    dump!(out, 7, args[0]);
                    if let Data::Lfb(lfb) = call.extra {
                        dump_lfb(&out, &lfb);
                    }
                    num_frame += 1;
                    if num_frame > max_frame {
                        // grGlideShutdown()
                        dump!(out, 1);
                        break;
                    }
                }
                Opcode::LfbBegin => dump!(out, 8),
                Opcode::LfbEnd => {
                    if let Data::Lfb(lfb) = call.extra {
                        dump!(out, 9);
                        dump_lfb(&out, &lfb);
                    }
                }
                Opcode::LfbBypassMode => dump!(out, 10, args[0]),
                Opcode::LfbWriteMode => dump!(out, 11, args[0]),
                Opcode::LfbGetWritePtr => dump!(out, 12, args[0]),
                Opcode::TexMinAddress => dump!(out, 13, args[0]),
                Opcode::TexMaxAddress => dump!(out, 14, args[0]),
                Opcode::TexTextureMemRequired => {
                    dump!(out, 15, args[0], args[1], args[2], args[3], args[4])
                }
                Opcode::TexDownloadMipMap => {
                    if let Data::Texture(tex) = call.extra {
                        dump!(out, 16, args[0], args[1], args[2]);
                        tex.dump(&out);
                    }
                }
                Opcode::TexSource => {
                    dump!(out, 17, args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
                }
                Opcode::TexCombineFunction => dump!(out, 18, args[0], args[1]),
                Opcode::DepthBufferMode => dump!(out, 19, args[0]),
                Opcode::CullMode => dump!(out, 20, args[0]),
                Opcode::ErrorSetCallback => dump!(out, 21, args[0]),
                Opcode::ClipWindow => dump!(out, 22, args[0], args[1], args[2], args[3]),
                Opcode::DrawTriangle => {
                    if let Data::Triangle(a, b, c) = call.extra {
                        dump!(out, 23);
                        dump_vertex(&out, &a);
                        dump_vertex(&out, &b);
                        dump_vertex(&out, &c);
                    }
                }
                Opcode::ChromakeyValue => dump!(out, 24, args[0]),
                Opcode::ChromakeyMode => dump!(out, 25, args[0]),
                Opcode::ConstantColorValue => dump!(out, 26, args[0]),
                Opcode::AlphaBlendFunction => dump!(out, 27, args[0], args[1], args[2], args[3]),
                Opcode::ColorCombineFunction => dump!(out, 28, args[0]),
                Opcode::AlphaSource => dump!(out, 29, args[0]),
                Opcode::DrawTriangleWithClip => {
                    if let Data::Triangle(a, b, c) = call.extra {
                        dump!(out, 30);
                        dump_vertex(&out, &a);
                        dump_vertex(&out, &b);
                        dump_vertex(&out, &c);
                    }
                }
                Opcode::TEX => panic!("Not an opcode!"),
            }
        } else {
            break;
        }
    }
}
