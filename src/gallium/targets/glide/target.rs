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

//! This file exposes the Glide API as defined in the frontend, as the C API Glide programs expect.
//!
//! A macro is used to reduce the amount of manual typing required, but it is kept as simple as
//! possible so it doesn’t support all of the symbols.
//!
//! Since we aren’t using any external crate, we just rename println!() as debug!(), but a better
//! solution would be to use the log crate, so that it would interact better with other software.

#![feature(once_cell)]
#![allow(non_snake_case)]

extern crate glide_frontend as frontend;

use frontend::{gr, FxBool, Gr, HwConfiguration, State};
use std::lazy::Lazy;
use std::os::raw::c_char;
use std::println as debug;
use std::sync::{Arc, Mutex};

static mut GR: Lazy<Arc<Mutex<Option<Gr>>>> = Lazy::new(|| Arc::new(Mutex::new(None)));

#[no_mangle]
pub fn grGlideGetVersion(version: &mut [u8; 80]) {
    const VERSION: &str = "Glide Version 2.1.1 (Mesa)";
    debug!("[36;1mgrGlideGetVersion[0m() -> {:?}", VERSION);
    version.copy_from_slice(b"Glide Version 2.1.1 (Mesa)\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0");
}

#[no_mangle]
pub fn grSstQueryBoards(hwConfig: *mut HwConfiguration) -> FxBool {
    let hwConfig = unsafe { &mut *hwConfig };
    let result = Gr::sst_query_boards(hwConfig);
    debug!("[36;1mgrSstQueryBoards[0m([32mhwConfig[0m={hwConfig:?}) -> {result}");
    result
}

#[no_mangle]
pub fn grGlideInit() {
    debug!("[36;1mgrGlideInit[0m()");
    let mut gr = unsafe { GR.lock() }.unwrap();
    *gr = Some(Gr::init());
}

#[no_mangle]
pub fn grGlideShutdown() {
    debug!("[36;1mgrGlideShutdown[0m()");
    let mut gr = unsafe { GR.lock() }.unwrap();
    *gr = None;
}

#[no_mangle]
pub fn grSstQueryHardware(hwConfig: *mut HwConfiguration) -> FxBool {
    let hwConfig = unsafe { &mut *hwConfig };
    let gr = unsafe { GR.lock() }.unwrap();
    if let Some(gr) = (*gr).as_ref() {
        let result = gr.sst_query_hardware(hwConfig);
        debug!("[36;1mgrSstQueryHardware[0m([32mhwConfig[0m={hwConfig:?}) -> {result}");
        result
    } else {
        panic!("grGlideGetState() called before grGlideInit()!");
    }
}

#[no_mangle]
pub fn grGlideGetState(arg: *mut State) {
    debug!("[36;1mgrGlideGetState[0m([32mstate[0m={:p})", arg);
    let gr = unsafe { GR.lock() }.unwrap();
    if let Some(gr) = (*gr).as_ref() {
        let state = gr.get_state();
        unsafe { std::ptr::copy(state, arg, 1) };
    } else {
        panic!("grGlideGetState() called before grGlideInit()!");
    }
}

#[no_mangle]
pub fn grGlideSetState(arg: *const State) {
    debug!("[36;1mgrGlideSetState[0m([32mstate[0m={:p})", arg);
    let mut gr = unsafe { GR.lock() }.unwrap();
    if let Some(gr) = (*gr).as_mut() {
        let state = gr.get_mut_state();
        unsafe { std::ptr::copy(arg, state, 1) };
    } else {
        panic!("grGlideGetState() called before grGlideInit()!");
    }
}

macro_rules! define_function {
    ($func:ident => $api:ident($($arg:ident: $arg_type:ty),*)) => {
        define_function!($func => $api($($arg: $arg_type),*) -> ());
    };
    ($func:ident => $api:ident($($arg:ident: $arg_type:ty),*), $debug:expr) => {
        define_function!($func => $api($($arg: $arg_type),*) -> (), $debug);
    };
    ($func:ident => $api:ident($($arg:ident: $arg_type:ty),*) -> Option<$type:ty>) => {
        #[no_mangle]
        pub fn $api($($arg: $arg_type),*) -> FxBool {
            let mut gr = unsafe { GR.lock() }.unwrap();
            if let Some(gr) = (*gr).as_mut() {
                let result = gr.$func($($arg),*).is_some().into();
                debug!(concat!("[36;1m", stringify!($api), "[0m(", $("[32m", stringify!($arg), "[0m={:?}, "),*, ") -> {:?}"), $($arg),*, result);
                result
            } else {
                panic!(concat!(stringify!($api), "() called before grGlideInit()!"));
            }
        }
    };
    ($func:ident => $api:ident($($arg:ident: $arg_type:ty),*) -> $type:ty) => {
        define_function!($func => $api($($arg: $arg_type),*) -> $type, concat!($("[32m", stringify!($arg), "[0m={:?}, "),*));
    };
    ($func:ident => $api:ident($($arg:ident: $arg_type:ty),*) -> $type:ty, $debug:expr) => {
        #[no_mangle]
        pub fn $api($($arg: $arg_type),*) -> $type {
            debug!(concat!("[36;1m", stringify!($api), "[0m(", $debug, ")"), $($arg),*);
            let mut gr = unsafe { GR.lock() }.unwrap();
            if let Some(gr) = (*gr).as_mut() {
                gr.$func($($arg),*)
            } else {
                panic!(concat!(stringify!($api), "() called before grGlideInit()!"));
            }
        }
    };
}

define_function!(sst_select => grSstSelect(which_sst: i32) -> Option<()>);
define_function!(sst_origin => grSstOrigin(origin: gr::OriginLocation));
define_function!(sst_win_open => grSstWinOpen(hwnd: u32, res: gr::ScreenResolution, refresh: gr::ScreenRefresh, cformat: gr::ColorFormat, origin: gr::OriginLocation, num_buffers: i32, num_aux_buffers: i32) -> Option<()>);
define_function!(sst_win_close => grSstWinClose());
define_function!(buffer_clear => grBufferClear(color: gr::Color, alpha: gr::Alpha, depth: u16));
define_function!(buffer_swap => grBufferSwap(swap_interval: i32));
define_function!(constant_color_value => grConstantColorValue(color: gr::Color));
define_function!(depth_buffer_mode => grDepthBufferMode(mode: gr::DepthBufferMode));
define_function!(depth_buffer_function => grDepthBufferFunction(func: gr::CmpFnc));
define_function!(depth_mask => grDepthMask(mask: bool));
define_function!(cull_mode => grCullMode(mode: gr::CullMode));
define_function!(clip_window => grClipWindow(minx: u32, miny: u32, maxx: u32, maxy: u32));
define_function!(color_combine => grColorCombine(func: gr::CombineFunction, factor: gr::CombineFactor, local: gr::CombineLocal, other: gr::CombineOther, invert: FxBool));
define_function!(alpha_combine => grAlphaCombine(func: gr::CombineFunction, factor: gr::CombineFactor, local: gr::CombineLocal, other: gr::CombineOther, invert: FxBool));
define_function!(tex_combine => grTexCombine(tmu: gr::ChipID, rgb_func: gr::CombineFunction, rgb_factor: gr::CombineFactor, alpha_func: gr::CombineFunction, alpha_factor: gr::CombineFactor, rgb_invert: FxBool, alpha_invert: FxBool));
define_function!(alpha_blend_function => grAlphaBlendFunction(rgb_sf: gr::AlphaBlendFnc, rgb_df: gr::AlphaBlendFnc, alpha_sf: gr::AlphaBlendFnc, alpha_df: gr::AlphaBlendFnc));
define_function!(alpha_test_function => grAlphaTestFunction(function: gr::CmpFnc));
define_function!(tex_filter_mode => grTexFilterMode(tmu: gr::ChipID, minFilterMode: gr::TextureFilterMode, magFilterMode: gr::TextureFilterMode));
define_function!(tex_mipmap_mode => grTexMipMapMode(tmu: gr::ChipID, mode: gr::MipMapMode, lodBlend: FxBool));
define_function!(alpha_test_reference_value => grAlphaTestReferenceValue(value: gr::Alpha));

#[no_mangle]
pub fn grDrawPoint(p: *const gr::Vertex) {
    let p = unsafe { &*p };
    debug!("[36;1mgrDrawPoint[0m([32mp[0m={p:?})");
    let mut gr = unsafe { GR.lock() }.unwrap();
    if let Some(gr) = (*gr).as_mut() {
        gr.draw_point(p)
    } else {
        panic!("grDrawPoint() called before grGlideInit()!");
    }
}

#[no_mangle]
pub fn grDrawLine(a: *const gr::Vertex, b: *const gr::Vertex) {
    let a = unsafe { &*a };
    let b = unsafe { &*b };
    debug!("[36;1mgrDrawLine[0m([32ma[0m={a:?}, [32mb[0m={b:?})");
    let mut gr = unsafe { GR.lock() }.unwrap();
    if let Some(gr) = (*gr).as_mut() {
        gr.draw_line(a, b)
    } else {
        panic!("grDrawLine() called before grGlideInit()!");
    }
}

#[no_mangle]
pub fn grDrawTriangle(a: *const gr::Vertex, b: *const gr::Vertex, c: *const gr::Vertex) {
    let a = unsafe { &*a };
    let b = unsafe { &*b };
    let c = unsafe { &*c };
    debug!("[36;1mgrDrawTriangle[0m([32ma[0m={a:?}, [32mb[0m={b:?}, [32mc[0m={c:?})");
    let mut gr = unsafe { GR.lock() }.unwrap();
    if let Some(gr) = (*gr).as_mut() {
        gr.draw_triangle(a, b, c)
    } else {
        panic!("grDrawTriangle() called before grGlideInit()!");
    }
}

#[no_mangle]
pub fn grTexCalcMemRequired(
    smallLod: gr::LOD,
    largeLod: gr::LOD,
    aspect: gr::AspectRatio,
    format: gr::TextureFormat,
) -> u32 {
    let info = gr::TexInfo::empty(smallLod, largeLod, aspect, format);
    let size = info.get_size();
    debug!(
        "[36;1mgrTexCalcMemRequired[0m([32msmallLod[0m={smallLod:?}, [32mlargeLod[0m={largeLod:?}, [32maspect[0m={aspect:?}, [32mformat[0m={format:?}) -> {size}");
    size as u32
}

#[no_mangle]
pub fn grTexTextureMemRequired(evenOdd: u32, info: *mut gr::TexInfo) -> u32 {
    let info = unsafe { &*info };
    let size = info.get_size();
    debug!(
        "[36;1mgrTexCalcMemRequired[0m([32mevenOdd[0m={evenOdd}, [32minfo[0m={info:?}) -> {size}"
    );
    size as u32
}

#[no_mangle]
pub fn grTexMinAddress(tmu: gr::ChipID) -> u32 {
    // Fake 4 MiB of texture addressing space.
    let addr = 0x0010_0000;
    debug!("[36;1mgrTexMinAddress[0m([32mtmu[0m={tmu:?}) -> 0x{addr:08x}");
    addr
}

#[no_mangle]
pub fn grTexMaxAddress(tmu: gr::ChipID) -> u32 {
    // Fake 4 MiB of texture addressing space.
    let addr = 0x0050_0000;
    debug!("[36;1mgrTexMaxAddress[0m([32mtmu[0m={tmu:?}) -> 0x{addr:08x}");
    addr
}

#[no_mangle]
pub fn grTexDownloadMipMap(
    tmu: gr::ChipID,
    startAddress: u32,
    evenOdd: u32,
    info: *mut gr::TexInfo,
) {
    let info = unsafe { &*info };
    debug!(
        "[36;1mgrTexDownloadMipMap[0m([32mtmu[0m={tmu:?}, [32mstartAddress[0m=0x{startAddress:08x}, [32mevenOdd[0m={evenOdd}, [32minfo[0m={info:?})"
    );
    let mut gr = unsafe { GR.lock() }.unwrap();
    if let Some(gr) = (*gr).as_mut() {
        gr.tex_download_mipmap(startAddress, evenOdd, info);
    }
}

#[no_mangle]
pub fn grTexSource(tmu: gr::ChipID, startAddress: u32, evenOdd: u32, info: *mut gr::TexInfo) {
    let info = unsafe { &*info };
    debug!(
        "[36;1mgrTexSource[0m([32mtmu[0m={tmu:?}, [32mstartAddress[0m=0x{startAddress:08x}, [32mevenOdd[0m={evenOdd}, [32minfo[0m={info:?})"
    );
    let mut gr = unsafe { GR.lock() }.unwrap();
    if let Some(gr) = (*gr).as_mut() {
        gr.tex_source(startAddress, evenOdd, info);
    }
}

// Compatibility functions for glide.dll

#[no_mangle]
pub fn grSstOpen(
    res: gr::ScreenResolution,
    refresh: gr::ScreenRefresh,
    cformat: gr::ColorFormat,
    origin: gr::OriginLocation,
    smoothing: u32,
    num_buffers: i32,
) -> FxBool {
    debug!("[31;1mgrSstOpen[0m([32mres[0m={res:?}, [32mrefresh[0m={refresh:?}, [32mcformat[0m={cformat:?}, [32morigin[0m={origin:?}, [32msmoothing[0m={smoothing}, [32mnum_buffers[0m={num_buffers})");
    let mut gr = unsafe { GR.lock() }.unwrap();
    if let Some(gr) = (*gr).as_mut() {
        gr.sst_win_open(0, res, refresh, cformat, origin, num_buffers, 1)
            .is_some()
            .into()
    } else {
        panic!(concat!(stringify!($api), "() called before grGlideInit()!"));
    }
}

define_function!(lfb_begin => grLfbBegin());
define_function!(lfb_end => grLfbEnd());
define_function!(lfb_bypass_mode => grLfbBypassMode(mode: gr::LfbBypassMode));
define_function!(lfb_write_mode => grLfbWriteMode(mode: gr::LfbWriteMode));
define_function!(lfb_get_write_ptr => grLfbGetWritePtr(buffer: gr::Buffer) -> *mut u8);
define_function!(chromakey_mode => grChromakeyMode(mode: gr::ChromakeyMode));
define_function!(chromakey_value => grChromakeyValue(value: gr::Color));
define_function!(sst_passthru_mode => grSstPassthruMode(mode: gr::PassthruMode));
define_function!(error_set_callback => grErrorSetCallback(func: extern "C" fn(string: *const c_char, fatal: FxBool)));
define_function!(tex_combine_function => grTexCombineFunction(tmu: gr::ChipID, func: gr::TextureCombineFnc));

// Utility functions, in the form gu*() instead of gr*().

define_function!(util_color_combine_function => guColorCombineFunction(func: gr::ColorCombineFnc));
define_function!(util_alpha_source => guAlphaSource(mode: gr::AlphaSource));

#[no_mangle]
pub fn guDrawTriangleWithClip(a: *const gr::Vertex, b: *const gr::Vertex, c: *const gr::Vertex) {
    let a = unsafe { &*a };
    let b = unsafe { &*b };
    let c = unsafe { &*c };
    debug!("[36;1mguDrawTriangleWithClip[0m([32ma[0m={a:?}, [32mb[0m={b:?}, [32mc[0m={c:?})");
    let mut gr = unsafe { GR.lock() }.unwrap();
    if let Some(gr) = (*gr).as_mut() {
        // XXX: this one should be its own function, and actually respect the clipping!
        gr.draw_triangle(a, b, c)
    } else {
        panic!("guDrawTriangleWithClip() called before grGlideInit()!");
    }
}
