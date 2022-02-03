use std::io::Write;
use crate::{AspectRatio, TexInfo, TextureFormat, LOD};
use nom::{
    bytes::streaming::{tag, take},
    combinator::{eof, verify},
    multi::count,
    number::streaming::{le_f32, le_u16, le_u32},
    sequence::tuple,
    IResult,
};

#[derive(Debug)]
pub enum Opcode {
    GlideInit,
    GlideShutdown,
    SstQueryHardware,
    SstSelect,
    SstOpen,
    SstPassthruMode,
    BufferClear,
    BufferSwap,
    LfbBegin,
    LfbEnd,
    LfbBypassMode,
    LfbWriteMode,
    LfbGetWritePtr,
    TexMinAddress,
    TexMaxAddress,
    TexTextureMemRequired,
    TexDownloadMipMap,
    TexSource,
    TexCombineFunction,
    DepthBufferMode,
    CullMode,
    ErrorSetCallback,
    ClipWindow,
    DrawTriangle,
    ChromakeyValue,
    ChromakeyMode,
    ColorCombineFunction,
    ConstantColorValue,
    AlphaSource,
    AlphaBlendFunction,
    DrawTriangleWithClip,

    TEX,
}

impl From<u32> for Opcode {
    fn from(opcode: u32) -> Opcode {
        match opcode {
            0 => Opcode::GlideInit,
            1 => Opcode::GlideShutdown,
            2 => Opcode::SstQueryHardware,
            3 => Opcode::SstSelect,
            4 => Opcode::SstOpen,
            5 => Opcode::SstPassthruMode,
            6 => Opcode::BufferClear,
            7 => Opcode::BufferSwap,
            8 => Opcode::LfbBegin,
            9 => Opcode::LfbEnd,
            10 => Opcode::LfbBypassMode,
            11 => Opcode::LfbWriteMode,
            12 => Opcode::LfbGetWritePtr,
            13 => Opcode::TexMinAddress,
            14 => Opcode::TexMaxAddress,
            15 => Opcode::TexTextureMemRequired,
            16 => Opcode::TexDownloadMipMap,
            17 => Opcode::TexSource,
            18 => Opcode::TexCombineFunction,
            19 => Opcode::DepthBufferMode,
            20 => Opcode::CullMode,
            21 => Opcode::ErrorSetCallback,
            22 => Opcode::ClipWindow,
            23 => Opcode::DrawTriangle,
            24 => Opcode::ChromakeyValue,
            25 => Opcode::ChromakeyMode,
            26 => Opcode::ConstantColorValue,
            27 => Opcode::AlphaBlendFunction,
            28 => Opcode::ColorCombineFunction,
            29 => Opcode::AlphaSource,
            30 => Opcode::DrawTriangleWithClip,

            0x5845_5467 => Opcode::TEX,
            _ => panic!("Unknown opcode {opcode}"),
        }
    }
}

impl Opcode {
    fn parse(i: &[u8]) -> IResult<&[u8], Opcode> {
        let (i, opcode) = le_u32(i)?;
        Ok((i, opcode.into()))
    }

    fn num_args(&self) -> usize {
        match self {
            Opcode::GlideInit => 0,
            Opcode::GlideShutdown => 0,
            Opcode::SstQueryHardware => 1,
            Opcode::SstSelect => 1,
            Opcode::SstOpen => 6,
            Opcode::SstPassthruMode => 1,
            Opcode::BufferClear => 3,
            Opcode::BufferSwap => 1,
            Opcode::LfbBegin => 0,
            Opcode::LfbEnd => 0,
            Opcode::LfbBypassMode => 1,
            Opcode::LfbWriteMode => 1,
            Opcode::LfbGetWritePtr => 1,
            Opcode::TexMinAddress => 1,
            Opcode::TexMaxAddress => 1,
            Opcode::TexTextureMemRequired => 5,
            Opcode::TexDownloadMipMap => 3,
            Opcode::TexSource => 7,
            Opcode::TexCombineFunction => 2,
            Opcode::DepthBufferMode => 1,
            Opcode::CullMode => 1,
            Opcode::ErrorSetCallback => 1,
            Opcode::ClipWindow => 4,
            Opcode::DrawTriangle => 0,
            Opcode::ChromakeyValue => 1,
            Opcode::ChromakeyMode => 1,
            Opcode::ConstantColorValue => 1,
            Opcode::AlphaBlendFunction => 4,
            Opcode::ColorCombineFunction => 1,
            Opcode::AlphaSource => 1,
            Opcode::DrawTriangleWithClip => 0,

            Opcode::TEX => panic!("Not an actual opcode!"),
        }
    }
}

pub fn header(i: &[u8]) -> IResult<&[u8], ()> {
    let (i, _) = tuple((tag(b"grTR"), verify(le_u32, |&version| version == 0)))(i)?;
    Ok((i, ()))
}

impl LOD {
    fn parse(i: &[u8]) -> IResult<&[u8], LOD> {
        let (i, lod) = le_u32(i)?;
        Ok((i, lod.into()))
    }
}

impl AspectRatio {
    fn parse(i: &[u8]) -> IResult<&[u8], AspectRatio> {
        let (i, aspect) = le_u32(i)?;
        Ok((i, aspect.into()))
    }
}

impl TextureFormat {
    fn parse(i: &[u8]) -> IResult<&[u8], TextureFormat> {
        let (i, format) = le_u32(i)?;
        Ok((i, format.into()))
    }
}

#[derive(Debug, Clone)]
pub struct Texture {
    small: LOD,
    large: LOD,
    aspect: AspectRatio,
    format: TextureFormat,
    data: Vec<u8>,
}

impl Texture {
    fn parse(i: &[u8]) -> IResult<&[u8], Texture> {
        let (i, _) = tag(b"gTEX")(i)?;
        let (i, (small, large, aspect, format)) = tuple((
            LOD::parse,
            LOD::parse,
            AspectRatio::parse,
            TextureFormat::parse,
        ))(i)?;
        let (width, height) = large.to_texture_size(aspect);
        let (i, data) = take(width * height * format.bytes_per_pixel())(i)?;
        Ok((
            i,
            Texture {
                small,
                large,
                aspect,
                format,
                data: data.to_vec(),
            },
        ))
    }

    pub fn dump<W: Write>(&self, mut out: W) {
        out.write_all(b"gTEX").unwrap();
        out.write_all(&(self.small as u32).to_le_bytes()).unwrap();
        out.write_all(&(self.large as u32).to_le_bytes()).unwrap();
        out.write_all(&(self.aspect as u32).to_le_bytes()).unwrap();
        out.write_all(&(self.format as u32).to_le_bytes()).unwrap();
        out.write_all(&self.data).unwrap();
    }

    pub fn as_tex_info(&self) -> TexInfo {
        TexInfo {
            small: self.small,
            large: self.large,
            aspect: self.aspect,
            format: self.format,
            data: self.data.as_ptr(),
        }
    }
}

#[derive(Debug)]
#[repr(C)]
pub struct Vertex(pub [f32; 15]);

impl Vertex {
    fn parse(i: &[u8]) -> IResult<&[u8], Vertex> {
        let (i, _) = tag(b"gVTX")(i)?;
        let (i, data) = count(le_f32, 15)(i)?;
        Ok((i, Vertex(data.try_into().unwrap())))
    }
}

#[derive(Debug)]
pub enum Data {
    Empty,
    Triangle(Vertex, Vertex, Vertex),
    Texture(Texture),
    Lfb(Vec<u16>),
}

impl Data {
    fn parse_lfb(i: &[u8]) -> IResult<&[u8], Data> {
        let (i, _) = tag(b"gLFB")(i)?;
        let (i, lfb) = count(le_u16, 1024 * 1024)(i)?;
        Ok((i, Data::Lfb(lfb)))
    }
}

#[derive(Debug)]
pub struct Call {
    pub opcode: Opcode,
    pub args: Vec<u32>,
    pub extra: Data,
}

impl Call {
    pub fn parse(i: &[u8]) -> IResult<&[u8], Option<Call>> {
        // TODO: that probably shouldn’t be here.
        if let Ok(_) = eof::<_, ()>(i) {
            return Ok((i, None));
        }
        let (i, opcode) = Opcode::parse(i)?;
        let num_args = opcode.num_args();
        let (i, args) = count(le_u32, num_args)(i)?;
        let (i, extra) = match opcode {
            Opcode::TexDownloadMipMap => {
                let (i, texture) = Texture::parse(i)?;
                (i, Data::Texture(texture))
            }
            Opcode::DrawTriangleWithClip | Opcode::DrawTriangle => {
                let (i, (a, b, c)) = tuple((Vertex::parse, Vertex::parse, Vertex::parse))(i)?;
                (i, Data::Triangle(a, b, c))
            }
            Opcode::BufferSwap | Opcode::LfbEnd => match Data::parse_lfb(i) {
                Ok(data) => data,
                Err(nom::Err::Error(_)) => (i, Data::Empty),
                err @ Err(_) => err?,
            },
            _ => (i, Data::Empty),
        };
        Ok((
            i,
            Some(Call {
                opcode,
                args,
                extra,
            }),
        ))
    }
}
