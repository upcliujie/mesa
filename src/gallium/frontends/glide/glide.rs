/**************************************************************************
 *
 * Copyright 2022 Emmanuel Gil Peyrot
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

//! This file contains the main part of the Glide driver.
//!
//! The only acceptable unsafe in this file is for interacting with user-provided data.

#![feature(untagged_unions)]
#![allow(non_snake_case, dead_code)]

extern crate gallium_sys as gallium;

pub mod gr;
mod pipe;

use gallium::{
    pipe_blendfactor, pipe_compare_func, pipe_map_flags, pipe_shader_type::PIPE_SHADER_FRAGMENT,
    PIPE_FACE_BACK, PIPE_FACE_FRONT, PIPE_FACE_NONE,
};
use pipe::{
    debug, tgsi, util, CsoContext, PipeBind, PipeBlendState, PipeBox, PipeClear, PipeColorUnion,
    PipeConstantBuffer, PipeContext, PipeDepthStencilAlphaState, PipeFormat, PipeFramebufferState,
    PipeLoader, PipeLoaderDevice, PipePrim, PipeRasterizerState, PipeResource,
    PipeResourceTemplate, PipeSamplerState, PipeSamplerViewTemplate, PipeScreen, PipeSurface,
    PipeSurfaceTemplate, PipeTextureTarget, PipeUsage, PipeVertexElement, PipeViewportState,
};
use std::collections::BTreeMap;
use std::ffi::{CStr, CString};
use std::fmt;
use std::os::raw::{c_char, c_void};
use std::ptr::null_mut;
use std::sync::atomic::{AtomicUsize, Ordering};

const GLIDE_NUM_TMU: usize = 2;

#[derive(Clone, Copy, PartialEq)]
#[repr(i32)]
pub enum FxBool {
    False = 0,
    True = 1,
}

impl Default for FxBool {
    fn default() -> FxBool {
        FxBool::False
    }
}

impl fmt::Debug for FxBool {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        if *self != FxBool::False {
            write!(fmt, "FXTRUE")
        } else {
            write!(fmt, "FXFALSE")
        }
    }
}

impl fmt::Display for FxBool {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        if *self != FxBool::False {
            write!(fmt, "true")
        } else {
            write!(fmt, "false")
        }
    }
}

impl From<bool> for FxBool {
    fn from(b: bool) -> FxBool {
        if b {
            Self::True
        } else {
            Self::False
        }
    }
}

#[derive(Debug, Default)]
struct FbiConfig {
    fbzColorPath: u32,
    fogMode: u32,
    alphaMode: u32,
    fbzMode: u32,
    lfbMode: u32,
    clipLeftRight: u32,
    clipBottomTop: u32,
    fogColor: u32,
    zaColor: u32,
    chromaKey: u32,
    stipple: u32,
    color0: u32,
    color1: u32,
}

type MipMapMode = i32;
type LOD = i32;
type NCCTable = u32;

#[derive(Debug, Default)]
struct TmuConfig {
    textureMode: u32,
    tLOD: u32,
    tDetail: u32,
    texBaseAddr: u32,
    texBaseAddr_1: u32,
    texBaseAddr_2: u32,
    texBaseAddr_3_8: u32,
    mmMode: MipMapMode,
    smallLod: LOD,
    largeLod: LOD,
    evenOdd: u32,
    nccTable: NCCTable,
}

type Hint = i32;
type MipMapId = u32;

// TODO: this struct should be 312 bytes in size, but we don’t really use it atm, nor do games it
// seems.

#[derive(Debug, Default)]
pub struct State {
    cull_mode: gr::CullMode,
    paramHints: Hint,
    fifoFree: i32,
    paramIndex: u32,
    tmuMask: u32,
    fbi_config: FbiConfig,
    tmu_config: [TmuConfig; GLIDE_NUM_TMU],
    ac_requires_it_alpha: FxBool,
    ac_requires_texture: FxBool,
    cc_requires_it_rgb: FxBool,
    cc_requires_texture: FxBool,
    cc_delta0mode: FxBool,
    allowLODdither: FxBool,
    checkFifo: FxBool,
    lfx_constant_depth: u16,
    lfb_constant_alpha: gr::Alpha,
    num_buffers: i32,
    color_format: gr::ColorFormat,
    current_mm: [MipMapId; GLIDE_NUM_TMU],
    clipwindowf_xmin: f32,
    clipwindowf_ymin: f32,
    clipwindowf_xmax: f32,
    clipwindowf_ymax: f32,
    screen_width: u32,
    screen_height: u32,
    a: f32,
    r: f32,
    g: f32,
    b: f32,
}

struct Gallium {
    screen: PipeScreen,
    pipe: PipeContext,
    cso: CsoContext,
    surface: PipeSurface,
    depth_surface: Option<PipeSurface>,
    framebuffer: PipeFramebufferState,
    depth_alpha: PipeDepthStencilAlphaState,
    write_ptr: Option<Vec<u8>>,

    vs: *mut c_void,
    fs: *mut c_void,
}

extern "C" fn error_callback(string: *const c_char, fatal: FxBool) {
    let qualifier = if fatal == FxBool::True {
        "Fatal"
    } else {
        "Recoverable"
    };
    // Safety: we are always the producer of this string, so it’s always UTF-8 encoded.
    let string = unsafe { CStr::from_ptr(string) }.to_str().unwrap();
    eprintln!("{qualifier} error: {string}");
}

pub struct Gr {
    state: State,
    devices: Vec<PipeLoaderDevice>,
    gallium: Option<Gallium>,
    error_callback: unsafe extern "C" fn(*const c_char, FxBool),
    textures: BTreeMap<u32, PipeResource>,
}

impl Gr {
    pub fn init() -> Gr {
        let devices = PipeLoader::probe();
        let textures = BTreeMap::new();
        println!("devices: {:?}", devices);
        Gr {
            state: Default::default(),
            devices,
            gallium: None,
            error_callback,
            textures,
        }
    }

    fn error<S: AsRef<str>>(&self, string: S, fatal: bool) {
        // Safety: we are always the producer of this string, so it’s always UTF-8 encoded.
        let c_string = CString::new(string.as_ref()).unwrap();
        unsafe { (self.error_callback)(c_string.as_ptr(), FxBool::from(fatal)) };
    }

    /// This function isn’t a method because it must be called before Gr::init(), to know whether
    /// and how many 3DFX cards are present in the system.
    pub fn sst_query_boards(hwConfig: &mut HwConfiguration) -> FxBool {
        let devices = PipeLoader::probe();
        let num_devices = devices.len();
        hwConfig.num_sst = num_devices as i32;
        FxBool::from(num_devices > 0)
    }

    pub fn sst_query_hardware(&self, hwConfig: &mut HwConfiguration) -> FxBool {
        let num_devices = self.devices.len();
        hwConfig.num_sst = num_devices as i32;
        for (i, _device) in self.devices.iter().enumerate() {
            let sst = &mut hwConfig.SSTs[i];
            // TODO: perhaps lie a little less about the capabilities of the actual GPU?
            sst.r#type = SstType::Voodoo;
            sst.sstBoard.a.fbRam = 4;
            sst.sstBoard.a.fbiRev = 2;
            sst.sstBoard.a.nTexelfx = 1;
            sst.sstBoard.a.sliDetect = FxBool::False;
            unsafe {
                sst.sstBoard.a.tmuConfig[0].tmuRev = 1;
                sst.sstBoard.a.tmuConfig[0].tmuRam = 4;
            }
        }
        FxBool::from(num_devices > 0)
    }

    pub fn sst_select(&mut self, which_sst: i32) -> Option<()> {
        //let which_sst = which_sst + 1;
        let num_devices = self.devices.len();
        if which_sst as usize >= num_devices {
            panic!("grSstSelect() must be lower than the number of devices.");
        }
        let device = &self.devices[which_sst as usize];
        let screen = device.create_screen()?;
        let pipe = screen.create_context()?;
        let cso = pipe.create_cso_context()?;
        let surface = PipeSurface::empty();
        let depth_surface = None;
        let framebuffer = PipeFramebufferState::empty();
        let depth_alpha = PipeDepthStencilAlphaState::new();
        let write_ptr = None;
        self.gallium = Some(Gallium {
            screen,
            pipe,
            cso,

            surface,
            depth_surface,
            framebuffer,
            depth_alpha,
            write_ptr,

            vs: null_mut(),
            fs: null_mut(),
        });
        Some(())
    }

    pub fn sst_win_open(
        &mut self,
        hwnd: u32,
        res: gr::ScreenResolution,
        refresh: gr::ScreenRefresh,
        cformat: gr::ColorFormat,
        origin: gr::OriginLocation,
        num_buffers: i32,
        num_aux_buffers: i32,
    ) -> Option<()> {
        assert_eq!(hwnd, 0);
        assert_eq!(refresh, gr::ScreenRefresh::R60Hz);
        assert_eq!(num_buffers, 2);
        assert_eq!(num_aux_buffers, 1);

        let (width, height) = res.into();
        self.state.screen_width = width as u32;
        self.state.screen_height = height as u32;

        // TODO: use this once we have winsys integration.
        //let refresh_rate: usize = refresh.into();

        self.state.color_format = cformat;
        // Iris only supports BGR565, nor RGB565.
        let pipe_format = PipeFormat::PIPE_FORMAT_B5G6R5_UNORM;
        if let Some(gallium) = &mut self.gallium {
            let cso = &gallium.cso;

            let template = PipeResourceTemplate::new(
                PipeTextureTarget::PIPE_TEXTURE_2D,
                pipe_format,
                width,
                height,
                PipeBind::RenderTarget,
                PipeUsage::Default,
            );
            let mut target = gallium.screen.create_resource(&template)?;
            let template = PipeSurfaceTemplate::new(pipe_format);
            gallium.surface = gallium.pipe.create_surface(&mut target, &template)?;
            gallium.framebuffer = PipeFramebufferState::new(width, height, &[&gallium.surface]);
            cso.set_framebuffer(&gallium.framebuffer);

            let flip = match origin {
                gr::OriginLocation::UpperLeft => false,
                gr::OriginLocation::LowerLeft => true,
                gr::OriginLocation::Any => panic!("OriginLocation::Any is currently unsupported."),
            };
            let viewport = PipeViewportState::new(0, 0, width as u32, height as u32, flip);
            cso.set_viewport(&viewport);

            let rasterizer = PipeRasterizerState::new(PIPE_FACE_NONE);
            cso.set_rasterizer(&rasterizer);

            let blend = PipeBlendState::new();
            cso.set_blend(&blend);

            cso.set_depth_stencil_alpha(&gallium.depth_alpha);

            let vs = "VERT
                DCL IN[0]
                DCL IN[1]
                DCL IN[2]
                DCL OUT[0], POSITION
                DCL OUT[1], COLOR
                DCL OUT[2], TEXCOORD[0]
                MOV OUT[0], IN[0]
                MOV OUT[1], IN[1]
                MOV OUT[2], IN[2]
                END\0";
            // Safety: if the shader is incorrect, better fail early!
            let tokens = tgsi::text_translate(vs).unwrap();
            let state = pipe::shader_state_from_tgsi(&tokens);
            gallium.vs = gallium.pipe.create_vs_state(&state);

            let fs = "FRAG
                PROPERTY FS_COLOR0_WRITES_ALL_CBUFS 1
                DCL IN[0], COLOR[0], PERSPECTIVE
                DCL OUT[0], COLOR[0]
                MOV OUT[0], IN[0]
                END\0";
            // Safety: if the shader is incorrect, better fail early!
            let tokens = tgsi::text_translate(fs).unwrap();
            let state = pipe::shader_state_from_tgsi(&tokens);
            gallium.fs = gallium.pipe.create_fs_state(&state);

            /*
            gallium.vs_flat = util::make_vertex_passthrough_shader(
                &gallium.pipe,
                [
                    gallium::tgsi_semantic::TGSI_SEMANTIC_POSITION,
                    gallium::tgsi_semantic::TGSI_SEMANTIC_COLOR,
                ],
                [0, 0],
                false,
            );
            */

            /*
            gallium.fs_passthrough = util::make_fragment_passthrough_shader(
                &gallium.pipe,
                gallium::tgsi_semantic::TGSI_SEMANTIC_COLOR,
                gallium::tgsi_interpolate_mode::TGSI_INTERPOLATE_PERSPECTIVE,
                true,
            );
            */

            /*
            gallium.vs_textured = util::make_vertex_passthrough_shader(
                &gallium.pipe,
                [
                    gallium::tgsi_semantic::TGSI_SEMANTIC_POSITION,
                    gallium::tgsi_semantic::TGSI_SEMANTIC_COLOR,
                    gallium::tgsi_semantic::TGSI_SEMANTIC_TEXCOORD,
                ],
                [0, 0, 0],
                false,
            );
            */
            /*
            let fs = util::make_fragment_tex_shader(
                &gallium.pipe,
                gallium::tgsi_interpolate_mode::TGSI_INTERPOLATE_LINEAR,
            );
            */
        }
        Some(())
    }

    pub fn sst_win_close(&mut self) {
        // Let everything get drop()ped here.
        self.gallium = None;
    }

    pub fn sst_origin(&mut self, origin: gr::OriginLocation) {
        let width = self.state.screen_width;
        let height = self.state.screen_height;
        if let Some(gallium) = &mut self.gallium {
            let flip = match origin {
                gr::OriginLocation::UpperLeft => false,
                gr::OriginLocation::LowerLeft => true,
                gr::OriginLocation::Any => panic!("OriginLocation::Any is currently unsupported."),
            };
            let viewport = PipeViewportState::new(0, 0, width, height, flip);
            gallium.cso.set_viewport(&viewport);
        }
    }

    pub fn get_state(&self) -> &State {
        &self.state
    }

    pub fn get_mut_state(&mut self) -> &mut State {
        &mut self.state
    }

    pub fn constant_color_value(&mut self, color: gr::Color) {
        let state = &mut self.state;
        let (r, g, b, a) = color.to_rgba_f(state.color_format);
        state.r = r;
        state.g = g;
        state.b = b;
        state.a = a;

        if let Some(gallium) = &mut self.gallium {
            let data: [f32; 4] = [r, g, b, 1.0];
            let buf = PipeConstantBuffer::from(&data);
            gallium
                .pipe
                .set_constant_buffer(PIPE_SHADER_FRAGMENT, 0, false, &buf);
        }
    }

    pub fn chromakey_mode(&self, mode: gr::ChromakeyMode) {
        // TODO: do something!
    }

    pub fn chromakey_value(&mut self, color: gr::Color) {
        let (r, g, b, a) = color.to_rgba_f(self.state.color_format);

        if let Some(gallium) = &mut self.gallium {
            let data: [f32; 4] = [r, g, b, 1.0];
            let buf = PipeConstantBuffer::from(&data);
            gallium
                .pipe
                .set_constant_buffer(PIPE_SHADER_FRAGMENT, 1, false, &buf);
        }
    }

    pub fn depth_buffer_mode(&mut self, mode: gr::DepthBufferMode) {
        if let Some(gallium) = &mut self.gallium {
            match mode {
                // XXX: do something!
                gr::DepthBufferMode::Disable => (),
                /*
                gr::DepthBufferMode::ZBuffer => {
                    let template = PipeResourceTemplate::new(
                        PipeTextureTarget::PIPE_TEXTURE_2D,
                        PipeFormat::PIPE_FORMAT_Z16_UNORM,
                        640,
                        480,
                        PipeBind::RenderTarget,
                        PipeUsage::Default,
                    );
                    let mut target = gallium.screen.create_resource(&template).unwrap();
                    let template = PipeSurfaceTemplate::new(PipeFormat::PIPE_FORMAT_Z16_UNORM);
                    let depth_surface =
                        gallium.pipe.create_surface(&mut target, &template).unwrap();
                    gallium.framebuffer.add_depth_buffer(&depth_surface);
                    gallium.cso.set_framebuffer(&gallium.framebuffer);
                    gallium.depth_surface = Some(depth_surface);
                }
                */
                _ => todo!("Unsupported depth buffer mode: {:?}", mode),
            }
        }
    }

    pub fn depth_mask(&mut self, mask: bool) {
        if let Some(gallium) = &mut self.gallium {
            gallium.depth_alpha.set_depth_writemask(mask);
            gallium.cso.set_depth_stencil_alpha(&gallium.depth_alpha);
        }
    }

    pub fn depth_buffer_function(&mut self, func: gr::CmpFnc) {
        if let Some(gallium) = &mut self.gallium {
            gallium.depth_alpha.set_depth_func(func.into());
            gallium.cso.set_depth_stencil_alpha(&gallium.depth_alpha);
        }
    }

    pub fn alpha_test_reference_value(&mut self, value: gr::Alpha) {
        if let Some(gallium) = &mut self.gallium {
            gallium.depth_alpha.set_alpha_ref_value(value.to_f32());
            gallium.cso.set_depth_stencil_alpha(&gallium.depth_alpha);
        }
    }

    pub fn alpha_test_function(&mut self, function: gr::CmpFnc) {
        if let Some(gallium) = &mut self.gallium {
            gallium.depth_alpha.set_alpha_func(function.into());
            gallium.cso.set_depth_stencil_alpha(&gallium.depth_alpha);
        }
    }

    /// Uploads texture data (and its mipmaps if info.small ≠ info.large) at the given address.  Of
    /// course, with Gallium we don’t expose direct addressing to the user API, so here we fake it
    /// with a binary tree.
    pub fn tex_download_mipmap(&mut self, startAddress: u32, evenOdd: u32, info: &gr::TexInfo) {
        //static mut TEXTURE_NUMBER: AtomicUsize = AtomicUsize::new(0);

        let (width, height) = info.get_dimensions();
        let format = info.format();
        let bpp = format.bytes_per_pixel();
        let pipe_format = PipeFormat::from(format);
        let data = info.get_data();

        if let Some(gallium) = &self.gallium {
            let template = PipeResourceTemplate::new(
                PipeTextureTarget::PIPE_TEXTURE_2D,
                pipe_format,
                width,
                height,
                PipeBind::Blendable,
                PipeUsage::Default,
            );
            // Safety: better fail here if we were unable to create the texture.
            let texture = gallium.screen.create_resource(&template).unwrap();
            // This clone doesn’t copy anything, it is implemented as a reference count increase.
            self.textures.insert(startAddress, texture.clone());

            {
                let box_ = PipeBox::new_2d(0, 0, width, height);
                let mut texture_map =
                    gallium
                        .pipe
                        .texture_map(&texture, pipe_map_flags::PIPE_MAP_WRITE, &box_);
                for (i, row) in texture_map.rows_mut().enumerate() {
                    let data_row = &data[i * width * 2..(i + 1) * width * 2];
                    row[..width * bpp].copy_from_slice(data_row);
                }

                /*
                // Only for debugging.
                let num = unsafe { TEXTURE_NUMBER.fetch_add(1, Ordering::SeqCst) };
                {
                    use std::fs::File;
                    use std::io::Write;
                    let mut raw = File::create(format!("texture-{num:03}.raw")).unwrap();
                    raw.write_all(data).unwrap();
                }
                debug::dump_transfer_bmp(
                    &gallium.pipe,
                    format!("texture-{num:03}.bmp"),
                    &texture_map,
                );
                */

                // texture_map will get unmapped once out of scope.
            }
        }
    }

    pub fn tex_source(&mut self, startAddress: u32, evenOdd: u32, info: &gr::TexInfo) {
        // Safety: the user must have uploaded a texture at this address, if not we better panic.
        let texture = self.textures.get(&startAddress).unwrap();

        if let Some(gallium) = &self.gallium {
            let sampler = PipeSamplerState::new();
            gallium.cso.set_samplers(PIPE_SHADER_FRAGMENT, &[&sampler]);
            let mut template = PipeSamplerViewTemplate::new();
            util::sampler_view_default_template(&mut template, &texture, texture.format());
            // Safety: better panic if we couldn’t create the sampler view.
            let view = gallium
                .pipe
                .create_sampler_view(&texture, &template)
                .unwrap();
            gallium
                .pipe
                .set_sampler_views(PIPE_SHADER_FRAGMENT, 0, 0, false, &[view]);
        }
    }

    pub fn buffer_clear(&mut self, color: gr::Color, alpha: gr::Alpha, depth: u16) {
        if let Some(gallium) = &self.gallium {
            let (r, g, b, _a) = color.to_rgba_f(self.state.color_format);
            let a = 1.; // It seems alpha is ignored here.
            let clear_color = PipeColorUnion::new_f(r, g, b, a);
            // TODO: clear depth too, if enabled.
            // TODO: clear alpha auxiliary buffer too, if enabled.
            gallium
                .pipe
                .clear(PipeClear::Color, &clear_color, (depth as f64) / 65535.);
            if let Some(depth_surface) = &gallium.depth_surface {
                gallium
                    .pipe
                    .clear(PipeClear::Depth, &clear_color, (depth as f64) / 65535.);
            }
        }
    }

    pub fn buffer_swap(&mut self, swap_interval: i32) {
        static mut FRAME_NUMBER: AtomicUsize = AtomicUsize::new(0);
        let width = self.state.screen_width as usize;
        let height = self.state.screen_height as usize;

        if let Some(gallium) = &mut self.gallium {
            if let Some(data) = &gallium.write_ptr {
                let box_ = PipeBox::new_2d(0, 0, width, height);
                // Safety: there is always a color texture bound in the framebuffer.
                let surface = &gallium.framebuffer.cbuf(0).unwrap();
                let texture = surface.texture();
                let mut texture_map =
                    gallium
                        .pipe
                        .texture_map(&texture, pipe_map_flags::PIPE_MAP_WRITE, &box_);
                for (i, row) in texture_map.rows_mut().enumerate() {
                    let data_row = &data[i * 1024 * 2..i * 1024 * 2 + row.len()];
                    row.copy_from_slice(data_row);
                }
            }

            gallium.pipe.flush(1);

            // TODO: replace that with actual winsys integration.
            let frame = unsafe { FRAME_NUMBER.fetch_add(1, Ordering::SeqCst) };
            debug::dump_surface_bmp(
                &gallium.pipe,
                format!("frame-{frame:03}.bmp"),
                &gallium.framebuffer,
            );
        }
    }

    pub fn cull_mode(&mut self, mode: gr::CullMode) {
        self.state.cull_mode = mode;

        if let Some(gallium) = &self.gallium {
            // TODO: depending on the origin, positive and negative are inverted, I think.
            let face = match mode {
                gr::CullMode::Disable => PIPE_FACE_NONE,
                gr::CullMode::Positive => PIPE_FACE_FRONT,
                gr::CullMode::Negative => PIPE_FACE_BACK,
            };
            let rasterizer = PipeRasterizerState::new(face);
            gallium.cso.set_rasterizer(&rasterizer);
        }
    }

    pub fn clip_window(&mut self, minx: u32, miny: u32, maxx: u32, maxy: u32) {
        self.state.clipwindowf_xmin = minx as f32;
        self.state.clipwindowf_xmax = maxx as f32;
        self.state.clipwindowf_ymin = miny as f32;
        self.state.clipwindowf_ymax = maxy as f32;
    }

    pub fn color_combine(
        &mut self,
        func: gr::CombineFunction,
        factor: gr::CombineFactor,
        local: gr::CombineLocal,
        other: gr::CombineOther,
        invert: FxBool,
    ) {
        if let Some(gallium) = &mut self.gallium {
            // TODO: see the comment in util_color_combine_function().
            let fs = match (func, factor, local, other) {
                (gr::CombineFunction::Zero, _, _, _) => {
                    "FRAG
                    DCL OUT[0], COLOR[0]
                    IMM[0] FLT32 { 0.0, 0.0, 0.0, 1.0}
                    MOV OUT[0], IMM[0]
                    END\0"
                }
                (gr::CombineFunction::Local, _, gr::CombineLocal::Iterated, _) => {
                    "FRAG
                    DCL IN[0], COLOR[0], PERSPECTIVE
                    DCL OUT[0], COLOR[0]
                    MOV OUT[0], IN[0]
                    END\0"
                }
                (gr::CombineFunction::Local, _, gr::CombineLocal::Constant, _) => {
                    "FRAG
                    DCL CONST[0][0]
                    DCL OUT[0], COLOR[0]
                    MOV OUT[0], CONST[0][0]
                    END\0"
                }
                (
                    gr::CombineFunction::ScaleOther,
                    gr::CombineFactor::One,
                    _,
                    gr::CombineOther::Texture,
                ) => {
                    "FRAG
                    DCL IN[0], TEXCOORD[0], PERSPECTIVE
                    DCL SAMP[0]
                    DCL OUT[0], COLOR[0]
                    TEX OUT[0], IN[0], SAMP[0], 2D
                    END\0"
                }
                (
                    gr::CombineFunction::ScaleOther,
                    gr::CombineFactor::Local,
                    gr::CombineLocal::Constant,
                    gr::CombineOther::Texture,
                ) => {
                    "FRAG
                    DCL IN[0], TEXCOORD[0], PERSPECTIVE
                    DCL CONST[0][0]
                    DCL SAMP[0]
                    DCL TEMP[0]
                    DCL OUT[0], COLOR[0]
                    TEX TEMP[0], IN[0], SAMP[0], 2D
                    MUL TEMP[0], TEMP[0].wwww, CONST[0][0].wwww
                    MOV OUT[0], TEMP[0]
                    END\0"
                }
                anything => panic!("Unhandled parameters for grColorCombine{:?}", anything),
            };
            // Safety: if the shader is incorrect, better fail early!
            let tokens = tgsi::text_translate(fs).unwrap();
            let state = pipe::shader_state_from_tgsi(&tokens);
            gallium.fs = gallium.pipe.create_fs_state(&state);
        }
    }

    fn draw(&self, primitive: PipePrim, vertices: Vec<[(f32, f32, f32, f32); 3]>) {
        let num_vertices = vertices.len();
        let num_attributes = 3;

        let velems = [
            PipeVertexElement::new(0, PipeFormat::PIPE_FORMAT_R32G32B32A32_FLOAT),
            PipeVertexElement::new(16, PipeFormat::PIPE_FORMAT_R32G32B32A32_FLOAT),
            PipeVertexElement::new(32, PipeFormat::PIPE_FORMAT_R32G32B32A32_FLOAT),
        ];
        let velem = pipe::CsoVelemsState::new(&velems);

        if let Some(gallium) = &self.gallium {
            // Safety: better fail early if we couldn’t create a buffer.
            let mut vbuf = gallium
                .screen
                .create_buffer(
                    PipeBind::VertexBuffer,
                    PipeUsage::Stream,
                    num_vertices * num_attributes * 4 * 4,
                )
                .unwrap();
            gallium.pipe.buffer_write(&mut vbuf, 0, vertices.as_slice());

            let vs = gallium.vs;
            let fs = gallium.fs;

            let cso = &gallium.cso;
            cso.set_vertex_shader_handle(vs);
            cso.set_fragment_shader_handle(fs);
            cso.set_vertex_elements(&velem);

            gallium.pipe.draw_vertex_buffer(
                &cso,
                &vbuf,
                0,
                0,
                primitive,
                num_vertices as u32,
                num_attributes as u32,
            );
        }
    }

    pub fn draw_point(&self, vertex: &gr::Vertex) {
        let mut vertices = Vec::with_capacity(1);
        let (s, t) = vertex.s_and_t();
        let gr::Vertex {
            x, y, r, g, b, a, ..
        } = vertex;
        vertices.push([
            // XXX: only working at 640×480!
            (x / 320. - 1., y / 240. - 1., 0., 1.),
            (r / 255., g / 255., b / 255., a / 255.),
            (s / 255., t / 255., 0., 1.),
        ]);
        self.draw(PipePrim::Points, vertices);
    }

    pub fn draw_line(&self, t1: &gr::Vertex, t2: &gr::Vertex) {
        let mut vertices = Vec::with_capacity(2);
        for vertex in [t1, t2] {
            let (s, t) = vertex.s_and_t();
            let gr::Vertex {
                x, y, r, g, b, a, ..
            } = vertex;
            vertices.push([
                // XXX: only working at 640×480!
                (x / 320. - 1., y / 240. - 1., 0., 1.),
                (r / 255., g / 255., b / 255., a / 255.),
                (s / 255., t / 255., 0., 1.),
            ]);
        }

        self.draw(PipePrim::Lines, vertices);
    }

    pub fn draw_triangle(&self, t1: &gr::Vertex, t2: &gr::Vertex, t3: &gr::Vertex) {
        let mut vertices = Vec::with_capacity(3);
        for vertex in [t1, t2, t3] {
            let (s, t) = vertex.s_and_t();
            let gr::Vertex {
                x, y, r, g, b, a, ..
            } = vertex;
            vertices.push([
                // XXX: only working at 640×480!
                (x / 320. - 1., y / 240. - 1., 0., 1.),
                (r / 255., g / 255., b / 255., a / 255.),
                (s / 255., t / 255., 0., 1.),
            ]);
        }

        self.draw(PipePrim::Triangles, vertices);
    }

    // Compatibility functions for glide.dll

    pub fn error_set_callback(
        &mut self,
        func: extern "C" fn(string: *const c_char, fatal: FxBool),
    ) {
        self.error_callback = func;
    }

    pub fn sst_passthru_mode(&self, mode: gr::PassthruMode) {
        if mode != gr::PassthruMode::Sst1 {
            self.error(format!("Unsupported passthru mode: {mode:?}"), true);
        }
    }

    pub fn lfb_begin(&mut self) {
        let width = self.state.screen_width as usize;
        let height = self.state.screen_height as usize;

        if let Some(gallium) = &mut self.gallium {
            gallium.pipe.flush(1);
            let box_ = PipeBox::new_2d(0, 0, width, height);
            // Safety: there is always a color texture bound in the framebuffer.
            let surface = &gallium.framebuffer.cbuf(0).unwrap();
            let texture = surface.texture();
            let texture_map =
                gallium
                    .pipe
                    .texture_map(&texture, pipe_map_flags::PIPE_MAP_READ, &box_);
            // This is magenta by default so we can find bugs more easily.
            let mut data = vec![0u8; 1024 * 1024 * 2];
            for (i, row) in texture_map.rows().enumerate() {
                let data_row = &mut data[i * 1024 * 2..i * 1024 * 2 + row.len()];
                data_row.copy_from_slice(row);
            }
            gallium.write_ptr = Some(data);
        }
    }

    pub fn lfb_end(&mut self) {
        let width = self.state.screen_width as usize;
        let height = self.state.screen_height as usize;

        if let Some(gallium) = &mut self.gallium {
            let box_ = PipeBox::new_2d(0, 0, width, height);
            // Safety: there is always a color texture bound in the framebuffer.
            let surface = &gallium.framebuffer.cbuf(0).unwrap();
            let texture = surface.texture();
            let mut texture_map =
                gallium
                    .pipe
                    .texture_map(&texture, pipe_map_flags::PIPE_MAP_WRITE, &box_);
            if let Some(data) = &gallium.write_ptr {
                for (i, row) in texture_map.rows_mut().enumerate() {
                    let data_row = &data[i * 1024 * 2..i * 1024 * 2 + row.len()];
                    row.copy_from_slice(data_row);
                }
            }
            gallium.write_ptr = None;
        }
    }

    pub fn lfb_bypass_mode(&self, mode: gr::LfbBypassMode) {
        // TODO: prevent all non-lfb draw operations.
    }

    pub fn lfb_write_mode(&self, mode: gr::LfbWriteMode) {
        if mode != gr::LfbWriteMode::Rgb565 {
            self.error(format!("Unsupported write mode: {mode:?}"), true);
        }
    }

    pub fn lfb_get_write_ptr(&mut self, buffer: gr::Buffer) -> *mut u8 {
        if buffer != gr::Buffer::Back {
            self.error(format!("Unsupported buffer: {buffer:?}"), true);
            return null_mut();
        }
        if let Some(gallium) = &mut self.gallium {
            if let Some(data) = &mut gallium.write_ptr {
                data.as_mut_ptr()
            } else {
                null_mut()
            }
        } else {
            null_mut()
        }
    }

    pub fn util_color_combine_function(&mut self, value: gr::ColorCombineFnc) {
        if let Some(gallium) = &mut self.gallium {
            // TODO: none of these shaders are (completely) correct, they all depend on state
            // outside of this function, but were enough to support a first game.
            //
            // COLOR[0] is the iterated color/alpha, TEXCOORD[0].st is the texture coordinate,
            // CONST[0][0] is the constant color/alpha, CONST[1][0] is the chromakey value, and
            // SAMP[0] is the bound texture.
            //
            // Shaders should be constructed from the state, for instance the SEQ/DP3/SEQ/MUL/
            // KILL_IF sequence should be inserted only if the chromakey is enabled.
            //
            // Similarly, the TEX instruction should actually depend on the TMU configuration, see
            // the tex_combine* functions.
            //
            // Another venue for improvement would be to switch to NIR, but the nir_builder pattern
            // is defined as static inline in C headers, which makes it unusable with bindgen.
            let fs = match value {
                gr::ColorCombineFnc::Ccrgb => {
                    "FRAG
                    DCL IN[0], COLOR[0], PERSPECTIVE
                    DCL CONST[0][0]
                    DCL TEMP[0]
                    DCL OUT[0], COLOR[0]
                    MOV TEMP[0].xyz, CONST[0][0]
                    MOV TEMP[0].w, IN[0].wwww
                    MOV OUT[0], TEMP[0]
                    END\0"
                }
                gr::ColorCombineFnc::Itrgb => {
                    "FRAG
                    DCL IN[0], COLOR[0], PERSPECTIVE
                    DCL OUT[0], COLOR[0]
                    MOV OUT[0], IN[0]
                    END\0"
                }
                gr::ColorCombineFnc::TextureTimesCcrgb => {
                    "FRAG
                    DCL IN[0], TEXCOORD[0], PERSPECTIVE
                    DCL CONST[0][0]
                    DCL CONST[1][0]
                    DCL SAMP[0]
                    DCL TEMP[0..1]
                    DCL OUT[0], COLOR[0]
                    IMM[0] FLT32 { -1.0, -1.0, -1.0, 1.0}
                    IMM[1] FLT32 { 3.0, 0.0, 0.0, 1.0}
                    TEX TEMP[0], IN[0], SAMP[0], 2D
                    SEQ TEMP[1], TEMP[0], CONST[1][0]
                    DP3 TEMP[1], TEMP[1], TEMP[1]
                    SEQ TEMP[1], TEMP[1], IMM[1]
                    MUL TEMP[1], TEMP[1], IMM[0]
                    KILL_IF TEMP[1].xxxx
                    MUL OUT[0], TEMP[0], CONST[0][0]
                    END\0"
                }
                gr::ColorCombineFnc::TextureTimesItrgb => {
                    "FRAG
                    DCL IN[0], TEXCOORD[0], PERSPECTIVE
                    DCL IN[1], COLOR[0], PERSPECTIVE
                    DCL CONST[1][0]
                    DCL SAMP[0]
                    DCL TEMP[0..1]
                    DCL OUT[0], COLOR[0]
                    IMM[0] FLT32 { -1.0, -1.0, -1.0, 1.0}
                    IMM[1] FLT32 { 3.0, 0.0, 0.0, 1.0}
                    TEX TEMP[0], IN[0], SAMP[0], 2D
                    SEQ TEMP[1], TEMP[0], CONST[1][0]
                    DP3 TEMP[1], TEMP[1], TEMP[1]
                    SEQ TEMP[1], TEMP[1], IMM[1]
                    MUL TEMP[1], TEMP[1], IMM[0]
                    KILL_IF TEMP[1].xxxx
                    MUL OUT[0], TEMP[0], IN[1]
                    END\0"
                }
                unhandled => panic!(
                    "Unhandled parameters for guColorCombineFunction({:?})",
                    unhandled
                ),
            };
            // Safety: if the shader is incorrect, better fail early!
            let tokens = tgsi::text_translate(fs).unwrap();
            let state = pipe::shader_state_from_tgsi(&tokens);
            gallium.fs = gallium.pipe.create_fs_state(&state);
        }
    }

    pub fn tex_combine(
        &self,
        tmu: gr::ChipID,
        rgb_func: gr::CombineFunction,
        rgb_factor: gr::CombineFactor,
        alpha_func: gr::CombineFunction,
        alpha_factor: gr::CombineFactor,
        rgb_invert: FxBool,
        alpha_invert: FxBool,
    ) {
        // TODO: do something!
    }

    pub fn tex_combine_function(&mut self, tmu: gr::ChipID, func: gr::TextureCombineFnc) {
        // TODO: this function configures what gets out of the TEX instruction in our shaders, but
        // I haven’t encountered any game which uses something else than Decal so far.
        match func {
            gr::TextureCombineFnc::Decal => (),
            unhandled => panic!(
                "Unhandled parameters for grTexCombineFunction({:?})",
                unhandled
            ),
        }
    }

    pub fn tex_filter_mode(
        &self,
        tmu: gr::ChipID,
        minFilterMode: gr::TextureFilterMode,
        magFilterMode: gr::TextureFilterMode,
    ) {
        // TODO: do something!
    }

    pub fn tex_mipmap_mode(&self, tmu: gr::ChipID, mode: gr::MipMapMode, lodBlend: FxBool) {
        // TODO: do something!
    }

    pub fn alpha_combine(
        &self,
        func: gr::CombineFunction,
        factor: gr::CombineFactor,
        local: gr::CombineLocal,
        other: gr::CombineOther,
        invert: FxBool,
    ) {
        // TODO: do something!
    }

    pub fn alpha_blend_function(
        &self,
        rgb_sf: gr::AlphaBlendFnc,
        rgb_df: gr::AlphaBlendFnc,
        alpha_sf: gr::AlphaBlendFnc,
        alpha_df: gr::AlphaBlendFnc,
    ) {
        if let Some(gallium) = &self.gallium {
            let mut blend = PipeBlendState::new();
            blend.set_factors(
                rgb_sf.into(),
                rgb_df.into(),
                alpha_sf.into(),
                alpha_df.into(),
            );
            gallium.cso.set_blend(&blend);
        }
    }

    pub fn util_alpha_source(&mut self, mode: gr::AlphaSource) {
        // TODO: do something actually!
        match mode {
            gr::AlphaSource::IteratedAlpha => (),
            unhandled => panic!("Unhandled parameters for guAlphaSource({:?})", unhandled),
        }
    }
}

const MAX_NUM_SST: usize = 4;

#[repr(i32)]
enum SstType {
    Voodoo = 0,
    Sst96 = 1,
    At3D = 2,
    Voodoo2 = 3,
}

#[derive(Debug)]
#[repr(C)]
struct TMUConfig {
    tmuRev: i32,
    tmuRam: i32,
}

#[derive(Debug)]
#[repr(C)]
struct VoodooConfig {
    fbRam: i32,
    fbiRev: i32,
    nTexelfx: i32,
    sliDetect: FxBool,
    tmuConfig: [TMUConfig; GLIDE_NUM_TMU],
}

#[derive(Debug)]
#[repr(C)]
struct Sst96Config {
    fbRam: i32,
    nTexelfx: i32,
    tmuConfig: TMUConfig,
}

#[derive(Debug)]
#[repr(C)]
struct AT3DConfig {
    rev: i32,
}

//type Voodoo2Config = VoodooConfig;

#[derive(Debug)]
#[repr(C)]
struct Voodoo2Config {
    fbRam: i32,
    fbiRev: i32,
    nTexelfx: i32,
    sliDetect: FxBool,
    tmuConfig: [TMUConfig; GLIDE_NUM_TMU],
}

union SstBoard {
    a: VoodooConfig,
    b: Sst96Config,
    c: AT3DConfig,
    d: Voodoo2Config,
}

struct SST {
    r#type: SstType,
    sstBoard: SstBoard,
}

impl fmt::Debug for SST {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        unsafe {
            match self.r#type {
                SstType::Voodoo => self.sstBoard.a.fmt(fmt),
                SstType::Sst96 => self.sstBoard.b.fmt(fmt),
                SstType::At3D => self.sstBoard.c.fmt(fmt),
                SstType::Voodoo2 => self.sstBoard.d.fmt(fmt),
            }
        }
    }
}

pub struct HwConfiguration {
    num_sst: i32,
    SSTs: [SST; MAX_NUM_SST],
}

impl fmt::Debug for HwConfiguration {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        let ssts: Vec<&SST> = self.SSTs.iter().take(self.num_sst as usize).collect();
        fmt.debug_struct("HwConfiguration")
            .field("num_sst", &self.num_sst)
            .field("SSTs", &ssts)
            .finish()
    }
}

impl From<gr::TextureFormat> for PipeFormat {
    fn from(fmt: gr::TextureFormat) -> PipeFormat {
        use PipeFormat::*;
        match fmt {
            gr::TextureFormat::Rgb332 => PIPE_FORMAT_R3G3B2_UNORM,
            //gr::TextureFormat::Yiq422 => ???,
            gr::TextureFormat::Alpha8 => PIPE_FORMAT_A8_UNORM,
            gr::TextureFormat::Intensity8 => PIPE_FORMAT_I8_UNORM,
            gr::TextureFormat::AlphaIntensity44 => PIPE_FORMAT_L4A4_UNORM,
            //gr::TextureFormat::P8 => PIPE_FORMAT_R8_UNORM,
            //gr::TextureFormat::Argb8332 => PIPE_FORMAT_A8R3G3B2_UNORM,
            //gr::TextureFormat::Ayiq8422 => ???,
            gr::TextureFormat::Rgb565 => PIPE_FORMAT_B5G6R5_UNORM, // XXX: should be RGB565, not BGR565!
            gr::TextureFormat::Argb1555 => PIPE_FORMAT_A1R5G5B5_UNORM,
            gr::TextureFormat::Argb4444 => PIPE_FORMAT_A4R4G4B4_UNORM,
            gr::TextureFormat::AlphaIntensity88 => PIPE_FORMAT_L8A8_UNORM,
            //gr::TextureFormat::Ap88 => PIPE_FORMAT_A8R8_UNORM,
            gr::TextureFormat::Rsvd0 | gr::TextureFormat::Rsvd1 | gr::TextureFormat::Rsvd2 => {
                panic!("Reserved texture format: {:?}", fmt)
            }
            _ => todo!("Unsupported texture format: {:?}", fmt),
        }
    }
}

impl From<gr::AlphaBlendFnc> for pipe_blendfactor {
    fn from(func: gr::AlphaBlendFnc) -> pipe_blendfactor {
        use gallium::pipe_blendfactor::*;
        match func {
            gr::AlphaBlendFnc::Zero => PIPE_BLENDFACTOR_ZERO,
            gr::AlphaBlendFnc::SrcAlpha => PIPE_BLENDFACTOR_SRC_ALPHA,
            gr::AlphaBlendFnc::SrcColor => PIPE_BLENDFACTOR_SRC_COLOR,
            gr::AlphaBlendFnc::DstAlpha => PIPE_BLENDFACTOR_DST_ALPHA,
            gr::AlphaBlendFnc::One => PIPE_BLENDFACTOR_ONE,
            gr::AlphaBlendFnc::OneMinusSrcAlpha => PIPE_BLENDFACTOR_INV_SRC_ALPHA,
            gr::AlphaBlendFnc::OneMinusSrcColor => PIPE_BLENDFACTOR_INV_SRC_COLOR,
            gr::AlphaBlendFnc::OneMinusDstAlpha => PIPE_BLENDFACTOR_INV_DST_ALPHA,
            //gr::AlphaBlendFnc::AlphaSaturate => PIPE_BLENDFACTOR_???,
            _ => todo!("Unsupported alpha blend function: {:?}", func),
        }
    }
}

impl From<gr::CmpFnc> for pipe_compare_func {
    fn from(func: gr::CmpFnc) -> pipe_compare_func {
        use gallium::pipe_compare_func::*;
        match func {
            gr::CmpFnc::Never => PIPE_FUNC_NEVER,
            gr::CmpFnc::Less => PIPE_FUNC_LESS,
            gr::CmpFnc::Equal => PIPE_FUNC_EQUAL,
            gr::CmpFnc::LEqual => PIPE_FUNC_LEQUAL,
            gr::CmpFnc::Greater => PIPE_FUNC_GREATER,
            gr::CmpFnc::NotEqual => PIPE_FUNC_NOTEQUAL,
            gr::CmpFnc::GEqual => PIPE_FUNC_GEQUAL,
            gr::CmpFnc::Always => PIPE_FUNC_ALWAYS,
        }
    }
}
