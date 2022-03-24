use core::ptr::null_mut;
use grtrace::parse::{Call, Data, Opcode, Vertex};
use grtrace::TexInfo;
use std::fs::File;
use std::io::{BufReader, Read};

#[link(name = "glide")]
extern "C" {
    fn grGlideInit();
    fn grGlideShutdown();
    fn grSstQueryHardware(hw_config: *mut u8) -> u32;
    fn grSstSelect(which: u32);
    fn grSstOpen(
        res: u32,
        refresh: u32,
        format: u32,
        origin: u32,
        smooth: u32,
        num_buffers: u32,
    ) -> u32;
    fn grSstPassthruMode(mode: u32);
    fn grBufferClear(color: u32, alpha: u32, depth: u32);
    fn grBufferSwap(interval: u32);
    fn grLfbBegin();
    fn grLfbEnd();
    fn grLfbBypassMode(mode: u32);
    fn grLfbWriteMode(mode: u32);
    fn grLfbGetWritePtr(buffer: u32) -> *mut u16;
    fn grTexMinAddress(tmu: u32);
    fn grTexMaxAddress(tmu: u32);
    fn grTexTextureMemRequired(evenOdd: u32, info: *const TexInfo);
    fn grTexDownloadMipMap(tmu: u32, startAddress: u32, evenOdd: u32, info: *const TexInfo);
    fn grTexSource(tmu: u32, startAddress: u32, evenOdd: u32, info: *const TexInfo);
    fn grTexCombineFunction(func: u32, factor: u32);
    fn grDepthBufferMode(mode: u32);
    fn grCullMode(mode: u32);
    //fn grErrorSetCallback(func: u32);
    fn grClipWindow(minx: u32, miny: u32, maxx: u32, maxy: u32);
    fn grDrawTriangle(a: *const Vertex, b: *const Vertex, c: *const Vertex);
    fn grChromakeyValue(value: u32);
    fn grChromakeyMode(mode: u32);
    fn grConstantColorValue(value: u32);
    fn grAlphaBlendFunction(a: u32, b: u32, c: u32, d: u32);
    fn guColorCombineFunction(func: u32);
    fn guAlphaSource(source: u32);
    fn guDrawTriangleWithClip(a: *const Vertex, b: *const Vertex, c: *const Vertex);
}

fn main() {
    let args: Vec<_> = std::env::args().collect();
    if args.len() != 2 {
        eprintln!("Usage: {} <file.grtrace>", args[0]);
        std::process::exit(1);
    }
    let filename = &args[1];
    let file = File::open(filename).unwrap();
    let mut reader = BufReader::new(file);
    let mut buf = Vec::new();
    reader.read_to_end(&mut buf).unwrap();

    let (mut i, _) = grtrace::parse::header(&buf).unwrap();
    let mut ptr: *mut u16 = null_mut();
    loop {
        let (i2, call) = Call::parse(i).unwrap();
        i = i2;
        if let Some(call) = call {
            let args = call.args;
            unsafe {
                match call.opcode {
                    Opcode::GlideInit => grGlideInit(),
                    Opcode::GlideShutdown => grGlideShutdown(),
                    Opcode::SstQueryHardware => {
                        let mut hw_config = [0u8; 64];
                        if grSstQueryHardware(hw_config.as_mut_ptr()) == 0 {
                            panic!("grSstQueryHardware() failed");
                        }
                    }
                    Opcode::SstSelect => grSstSelect(args[0]),
                    Opcode::SstOpen => {
                        if grSstOpen(args[0], args[1], args[2], args[3], args[4], args[5]) == 0 {
                            panic!("grSstOpen() failed");
                        }
                    }
                    Opcode::SstPassthruMode => grSstPassthruMode(args[0]),
                    Opcode::BufferClear => grBufferClear(args[0], args[1], args[2]),
                    Opcode::BufferSwap => {
                        if let Data::Lfb(lfb) = call.extra {
                            for i in 0..1024 * 1024 {
                                // If there was magenta in the stored picture, skip it.
                                if lfb[i] != 0xf81f {
                                    *ptr.add(i) = lfb[i];
                                }
                            }
                            ptr = null_mut();
                        }
                        grBufferSwap(args[0]);
                    }
                    Opcode::LfbBegin => grLfbBegin(),
                    Opcode::LfbEnd => {
                        if let Data::Lfb(lfb) = call.extra {
                            for i in 0..1024 * 1024 {
                                // If there was magenta in the stored picture, skip it.
                                if lfb[i] != 0xf81f {
                                    *ptr.add(i) = lfb[i];
                                }
                            }
                            ptr = null_mut();
                        }
                        grLfbEnd();
                    }
                    Opcode::LfbBypassMode => grLfbBypassMode(args[0]),
                    Opcode::LfbWriteMode => grLfbWriteMode(args[0]),
                    Opcode::LfbGetWritePtr => {
                        ptr = grLfbGetWritePtr(args[0]);
                    }
                    Opcode::TexMinAddress => grTexMinAddress(args[0]),
                    Opcode::TexMaxAddress => grTexMaxAddress(args[0]),
                    Opcode::TexTextureMemRequired => {
                        let info = TexInfo::new(args[1], args[2], args[3], args[4]);
                        grTexTextureMemRequired(args[0], &info);
                    }
                    Opcode::TexDownloadMipMap => {
                        if let Data::Texture(tex) = call.extra {
                            let info = tex.as_tex_info();
                            grTexDownloadMipMap(args[0], args[1], args[2], &info);
                        }
                    }
                    Opcode::TexSource => {
                        let info = TexInfo::new(args[3], args[4], args[5], args[6]);
                        grTexSource(args[0], args[1], args[2], &info);
                    }
                    Opcode::TexCombineFunction => grTexCombineFunction(args[0], args[1]),
                    Opcode::DepthBufferMode => grDepthBufferMode(args[0]),
                    Opcode::CullMode => grCullMode(args[0]),
                    Opcode::ErrorSetCallback => {
                        println!("grErrorSetCallback(0x{:08x})", args[0]);
                        //grErrorSetCallback(args[0]);
                    }
                    Opcode::ClipWindow => grClipWindow(args[0], args[1], args[2], args[3]),
                    Opcode::DrawTriangle => {
                        if let Data::Triangle(a, b, c) = call.extra {
                            grDrawTriangle(&a, &b, &c);
                        }
                    }
                    Opcode::ChromakeyValue => grChromakeyValue(args[0]),
                    Opcode::ChromakeyMode => grChromakeyMode(args[0]),
                    Opcode::ConstantColorValue => grConstantColorValue(args[0]),
                    Opcode::AlphaBlendFunction => {
                        grAlphaBlendFunction(args[0], args[1], args[2], args[3]);
                    }
                    Opcode::ColorCombineFunction => guColorCombineFunction(args[0]),
                    Opcode::AlphaSource => guAlphaSource(args[0]),
                    Opcode::DrawTriangleWithClip => {
                        if let Data::Triangle(a, b, c) = call.extra {
                            guDrawTriangleWithClip(&a, &b, &c);
                        }
                    }
                    Opcode::TEX => panic!("Not an opcode!"),
                }
            }
        } else {
            break;
        }
    }
}
