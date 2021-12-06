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

//! This file contains driver-independent bindings for the Gallium API.  It relies on automatically
//! generated raw bindings from bindgen (see meson.build and bindgen.h for how it works), and
//! exposes an API closer to how one might use Gallium from Rust.
//!
//! I tried to exprime the constraints of the API as part of the type system wherever possible, for
//! instance reference counting in pipe_resource is exposed in PipeResource as the Clone/Drop
//! couple of traits, so the Rust code doesn’t have to care about reference counting like C code
//! would.  Another example is the TextureMap binding, which provides safe access to texture rows,
//! taking the stride into account, and unmaps the texture on Drop.

use gallium::{
    cso_context, cso_create_context, cso_set_blend, cso_set_depth_stencil_alpha,
    cso_set_fragment_shader_handle, cso_set_framebuffer, cso_set_rasterizer, cso_set_samplers,
    cso_set_vertex_elements, cso_set_vertex_shader_handle, cso_set_viewport, cso_velems_state,
    pipe_blend_func, pipe_blend_state, pipe_blendfactor, pipe_box, pipe_color_union,
    pipe_compare_func, pipe_constant_buffer, pipe_context, pipe_depth_stencil_alpha_state,
    pipe_framebuffer_state, pipe_loader_create_screen, pipe_loader_device, pipe_loader_drm_probe,
    pipe_loader_probe, pipe_loader_release, pipe_loader_sw_probe, pipe_map_flags, pipe_prim_type,
    pipe_rasterizer_state, pipe_reference, pipe_resource, pipe_resource__bindgen_ty_1,
    pipe_resource_usage, pipe_rt_blend_state, pipe_sampler_state, pipe_sampler_view, pipe_screen,
    pipe_shader_type, pipe_stencil_state, pipe_surface, pipe_surface_desc,
    pipe_surface_desc__bindgen_ty_1, pipe_tex_mipfilter, pipe_tex_wrap, pipe_transfer,
    pipe_vertex_element, pipe_viewport_state, pipe_viewport_swizzle, util_draw_vertex_buffer,
    PIPE_BIND_BLENDABLE, PIPE_BIND_RENDER_TARGET, PIPE_BIND_VERTEX_BUFFER, PIPE_CLEAR_COLOR,
    PIPE_CLEAR_DEPTH, PIPE_CLEAR_STENCIL, PIPE_MASK_RGBA,
};
use std::convert::TryInto;
use std::ffi::CStr;
use std::fmt;
use std::marker::PhantomData;
use std::mem::MaybeUninit;
use std::os::raw::c_void;
use std::ptr::null_mut;

pub use gallium::pipe_format as PipeFormat;
pub use gallium::pipe_texture_target as PipeTextureTarget;

#[repr(C)]
pub struct PipeLoader;

impl PipeLoader {
    pub fn probe() -> Vec<PipeLoaderDevice> {
        unsafe {
            let max_num = pipe_loader_probe(null_mut(), 0);
            let mut data = Vec::with_capacity(max_num as usize);
            let num = pipe_loader_probe(data.as_mut_ptr(), max_num);
            assert!(num <= max_num);
            if num < max_num {
                println!("WARN: originally found {} devices, now {}.", max_num, num);
            }
            data.set_len(num as usize);
            std::mem::transmute(data)
        }
    }

    pub fn sw_probe() -> Vec<PipeLoaderDevice> {
        unsafe {
            let max_num = pipe_loader_sw_probe(null_mut(), 0);
            let mut data = Vec::with_capacity(max_num as usize);
            let num = pipe_loader_sw_probe(data.as_mut_ptr(), max_num);
            assert!(num <= max_num);
            if num < max_num {
                println!("WARN: originally found {} devices, now {}.", max_num, num);
            }
            data.set_len(num as usize);
            std::mem::transmute(data)
        }
    }

    pub fn drm_probe() -> Vec<PipeLoaderDevice> {
        unsafe {
            let max_num = pipe_loader_drm_probe(null_mut(), 0);
            let mut data = Vec::with_capacity(max_num as usize);
            let num = pipe_loader_drm_probe(data.as_mut_ptr(), max_num);
            assert!(num <= max_num);
            if num < max_num {
                println!("WARN: originally found {} devices, now {}.", max_num, num);
            }
            data.set_len(num as usize);
            std::mem::transmute(data)
        }
    }
}

pub struct PipeLoaderDevice {
    inner: *mut pipe_loader_device,
}

impl Drop for PipeLoaderDevice {
    fn drop(&mut self) {
        unsafe { pipe_loader_release(&mut self.inner, 1) };
    }
}

impl PipeLoaderDevice {
    pub fn create_screen(&self) -> Option<PipeScreen> {
        unsafe {
            let screen = pipe_loader_create_screen(self.inner);
            if screen.is_null() {
                None
            } else {
                Some(std::mem::transmute(screen))
            }
        }
    }

    /// # Safety
    /// We assume Mesa will always set the driver_name as a proper UTF-8 string.
    pub fn driver_name(&self) -> &str {
        let c_str = unsafe { CStr::from_ptr((*self.inner).driver_name) };
        c_str.to_str().unwrap()
    }
}

impl fmt::Debug for PipeLoaderDevice {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_tuple("PipeLoaderDevice")
            .field(&self.driver_name())
            .finish()
    }
}

#[derive(Debug)]
pub struct PipeScreen {
    inner: *mut pipe_screen,
}

impl PipeScreen {
    pub fn create_context(&self) -> Option<PipeContext> {
        unsafe {
            let context_create = (*self.inner).context_create.unwrap();
            let pipe = context_create(self.inner, null_mut(), 0);
            if pipe.is_null() {
                None
            } else {
                Some(std::mem::transmute(pipe))
            }
        }
    }

    pub fn create_resource(&self, template: &PipeResourceTemplate) -> Option<PipeResource> {
        unsafe {
            let resource_create = (*self.inner).resource_create.unwrap();
            let resource = resource_create(self.inner, &template.0);
            if resource.is_null() {
                None
            } else {
                Some(std::mem::transmute(resource))
            }
        }
    }

    pub fn create_buffer(
        &self,
        bind: PipeBind,
        usage: PipeUsage,
        size: usize,
    ) -> Option<PipeResource> {
        let template = PipeResourceTemplate::new(
            PipeTextureTarget::PIPE_BUFFER,
            PipeFormat::PIPE_FORMAT_R8_UNORM,
            size,
            1,
            bind,
            usage,
        );
        self.create_resource(&template)
    }
}

pub struct PipeContext {
    inner: *mut pipe_context,
}

impl fmt::Debug for PipeContext {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(fmt, "{:?}", unsafe { *self.inner })
    }
}

impl PipeContext {
    pub fn create_cso_context(&self) -> Option<CsoContext> {
        unsafe {
            let cso = cso_create_context(self.inner, 0);
            if cso.is_null() {
                None
            } else {
                Some(std::mem::transmute(cso))
            }
        }
    }

    pub fn create_surface(
        &self,
        target: &mut PipeResource,
        template: &PipeSurfaceTemplate,
    ) -> Option<PipeSurface> {
        unsafe {
            let create_surface = (*self.inner).create_surface.unwrap();
            let surface = create_surface(self.inner, target.inner, &template.0);
            if surface.is_null() {
                None
            } else {
                Some(std::mem::transmute(surface))
            }
        }
    }

    pub fn create_vs_state(&self, state: &pipe_shader_state) -> *mut c_void {
        unsafe {
            let create_vs_state = (*self.inner).create_vs_state.unwrap();
            create_vs_state(self.inner, state)
        }
    }

    pub fn create_fs_state(&self, state: &pipe_shader_state) -> *mut c_void {
        unsafe {
            let create_fs_state = (*self.inner).create_fs_state.unwrap();
            create_fs_state(self.inner, state)
        }
    }

    pub fn create_sampler_view(
        &self,
        texture: &PipeResource,
        template: &PipeSamplerViewTemplate,
    ) -> Option<PipeSamplerView> {
        unsafe {
            let create_sampler_view = (*self.inner).create_sampler_view.unwrap();
            let sampler_view = create_sampler_view(self.inner, texture.inner, &template.0);
            if sampler_view.is_null() {
                None
            } else {
                Some(std::mem::transmute(sampler_view))
            }
        }
    }

    pub fn set_sampler_views(
        &self,
        shader: pipe_shader_type,
        start_slot: u32,
        unbind_num_trailing_slots: u32,
        take_ownership: bool,
        views: &[PipeSamplerView],
    ) {
        let mut views: Vec<_> = views.iter().map(|view| view.inner).collect();
        unsafe {
            let set_sampler_views = (*self.inner).set_sampler_views.unwrap();
            set_sampler_views(
                self.inner,
                shader,
                start_slot,
                views.len() as u32,
                unbind_num_trailing_slots,
                take_ownership,
                views.as_mut_ptr(),
            );
        }
    }

    pub fn set_constant_buffer(
        &self,
        shader: pipe_shader_type,
        index: u32,
        take_ownership: bool,
        buf: &PipeConstantBuffer,
    ) {
        unsafe {
            let set_constant_buffer = (*self.inner).set_constant_buffer.unwrap();
            set_constant_buffer(self.inner, shader, index, take_ownership, &buf.0);
        }
    }

    pub fn flush(&self, flags: u32) {
        unsafe {
            let flush = (*self.inner).flush.unwrap();
            flush(self.inner, null_mut(), flags);
        }
    }

    pub fn clear(&self, what: PipeClear, color: &PipeColorUnion, depth: f64) {
        let what = match what {
            PipeClear::Color => PIPE_CLEAR_COLOR,
            PipeClear::Depth => PIPE_CLEAR_DEPTH,
            PipeClear::Stencil => PIPE_CLEAR_STENCIL,
        };
        unsafe {
            let clear = (*self.inner).clear.unwrap();
            clear(self.inner, what, null_mut(), &color.inner, depth, 0);
        }
    }

    pub fn buffer_write<T>(&self, buf: &PipeResource, offset: u32, data: &[T]) {
        let size = data.len() * std::mem::size_of::<T>();
        unsafe {
            let buffer_subdata = (*self.inner).buffer_subdata.unwrap();
            buffer_subdata(
                self.inner,
                buf.inner,
                pipe_map_flags::PIPE_MAP_WRITE as u32,
                offset,
                size as u32,
                data.as_ptr() as *const c_void,
            );
        }
    }

    pub fn texture_map(
        &self,
        texture: &PipeResource,
        flags: pipe_map_flags,
        box_: &PipeBox,
    ) -> TextureMap {
        let mut t = null_mut();
        unsafe {
            let texture_map = (*self.inner).texture_map.unwrap();
            let ptr = texture_map(self.inner, texture.inner, 0, flags as u32, &box_.0, &mut t);
            TextureMap { ptr, t, pipe: self }
        }
    }

    fn texture_unmap(&self, t: *mut pipe_transfer) {
        unsafe {
            let texture_unmap = (*self.inner).texture_unmap.unwrap();
            texture_unmap(self.inner, t);
        }
    }

    pub fn draw_vertex_buffer(
        &self,
        cso: &CsoContext,
        vbuf: &PipeResource,
        vbuf_slot: u32,
        offset: u32,
        prim_type: PipePrim,
        num_attribs: u32,
        num_verts: u32,
    ) {
        unsafe {
            util_draw_vertex_buffer(
                self.inner,
                cso.inner,
                vbuf.inner,
                vbuf_slot,
                offset,
                pipe_prim_type::from(prim_type) as u32,
                num_attribs,
                num_verts,
            )
        };
    }
}

pub struct TextureMap<'a> {
    ptr: *mut c_void,
    t: *mut pipe_transfer,
    pipe: &'a PipeContext,
}

impl Drop for TextureMap<'_> {
    fn drop(&mut self) {
        self.pipe.texture_unmap(self.t);
    }
}

pub struct TextureRowIterator<'a> {
    ptr: *const u8,
    stride: usize,
    height: usize,
    _data: PhantomData<TextureMap<'a>>,
}

impl<'a> Iterator for TextureRowIterator<'a> {
    type Item = &'a [u8];

    fn next(&mut self) -> Option<&'a [u8]> {
        if self.height > 0 {
            let ptr = self.ptr;
            self.height -= 1;
            self.ptr = unsafe { self.ptr.add(self.stride) };
            Some(unsafe { std::slice::from_raw_parts(ptr, self.stride) })
        } else {
            None
        }
    }
}

pub struct TextureRowIteratorMut<'a> {
    ptr: *mut u8,
    stride: usize,
    height: usize,
    _data: PhantomData<TextureMap<'a>>,
}

impl<'a> Iterator for TextureRowIteratorMut<'a> {
    type Item = &'a mut [u8];

    fn next(&mut self) -> Option<&'a mut [u8]> {
        if self.height != 0 {
            let ptr = self.ptr;
            self.height -= 1;
            self.ptr = unsafe { self.ptr.add(self.stride) };
            Some(unsafe { std::slice::from_raw_parts_mut(ptr, self.stride) })
        } else {
            None
        }
    }
}

impl<'a> TextureMap<'a> {
    pub fn rows(&self) -> TextureRowIterator<'a> {
        let t = unsafe { &*self.t };
        let resource = unsafe { &*t.resource };
        TextureRowIterator {
            ptr: self.ptr as *const u8,
            stride: t.stride as usize,
            height: resource.height0 as usize,
            _data: PhantomData,
        }
    }

    pub fn rows_mut(&mut self) -> TextureRowIteratorMut<'a> {
        let t = unsafe { &*self.t };
        let resource = unsafe { &*t.resource };
        TextureRowIteratorMut {
            ptr: self.ptr as *mut u8,
            stride: t.stride as usize,
            height: resource.height0 as usize,
            _data: PhantomData,
        }
    }
}

pub enum PipeBind {
    RenderTarget,
    Blendable,
    VertexBuffer,
}

#[derive(Debug)]
pub struct PipeResource {
    inner: *mut pipe_resource,
}

impl Clone for PipeResource {
    fn clone(&self) -> PipeResource {
        unsafe { (*self.inner).__bindgen_anon_1.reference.count += 1 };
        PipeResource { inner: self.inner }
    }
}

impl Drop for PipeResource {
    fn drop(&mut self) {
        unsafe { (*self.inner).__bindgen_anon_1.reference.count -= 1 };
    }
}

impl PipeResource {
    fn from(inner: *mut pipe_resource) -> PipeResource {
        unsafe { (*inner).__bindgen_anon_1.reference.count += 1 };
        PipeResource { inner }
    }

    pub fn format(&self) -> PipeFormat {
        unsafe { (*self.inner).format() }
    }
}

pub struct PipeResourceTemplate(pipe_resource);

impl PipeResourceTemplate {
    pub fn new(
        target: PipeTextureTarget,
        format: PipeFormat,
        width: usize,
        height: usize,
        bind: PipeBind,
        usage: PipeUsage,
    ) -> PipeResourceTemplate {
        let bind = match bind {
            PipeBind::RenderTarget => PIPE_BIND_RENDER_TARGET,
            PipeBind::Blendable => PIPE_BIND_BLENDABLE,
            PipeBind::VertexBuffer => PIPE_BIND_VERTEX_BUFFER,
        };
        let mut resource = pipe_resource {
            width0: width as u32,
            height0: height as u16,
            depth0: 1,
            array_size: 1,
            bind,

            __bindgen_anon_1: pipe_resource__bindgen_ty_1 {
                reference: pipe_reference { count: 0 },
            },
            _bitfield_align_1: Default::default(),
            _bitfield_1: Default::default(),
            next: null_mut(),
            screen: null_mut(),
            flags: Default::default(),
        };
        resource.set_target(target);
        resource.set_format(format);
        resource.set_last_level(0);
        resource.set_usage(pipe_resource_usage::from(usage) as u32);
        PipeResourceTemplate(resource)
    }
}

#[derive(Debug)]
pub struct PipeSurface {
    inner: *mut pipe_surface,
}

impl PipeSurface {
    pub fn empty() -> PipeSurface {
        PipeSurface { inner: null_mut() }
    }

    pub fn texture(&self) -> PipeResource {
        let inner = unsafe { *self.inner }.texture;
        PipeResource::from(inner)
    }
}

pub struct PipeSurfaceTemplate(pipe_surface);

impl PipeSurfaceTemplate {
    pub fn new(format: PipeFormat) -> PipeSurfaceTemplate {
        let mut tex = pipe_surface_desc__bindgen_ty_1 {
            level: 0,
            _bitfield_align_1: Default::default(),
            _bitfield_1: Default::default(),
        };
        tex.set_first_layer(0);
        tex.set_last_layer(0);
        let mut surface = pipe_surface {
            reference: pipe_reference { count: 0 },
            _bitfield_align_1: Default::default(),
            _bitfield_1: Default::default(),
            texture: null_mut(),
            context: null_mut(),
            width: 0,
            height: 0,
            _bitfield_2: Default::default(),
            _bitfield_align_2: Default::default(),
            u: pipe_surface_desc { tex },
        };
        surface.set_format(format);
        PipeSurfaceTemplate(surface)
    }
}

#[derive(Debug)]
pub struct CsoContext {
    inner: *mut cso_context,
}

impl CsoContext {
    pub fn set_framebuffer(&self, framebuffer: &PipeFramebufferState) {
        unsafe { cso_set_framebuffer(self.inner, &framebuffer.0) };
    }

    pub fn set_blend(&self, blend: &PipeBlendState) {
        unsafe { cso_set_blend(self.inner, &blend.0) };
    }

    pub fn set_depth_stencil_alpha(&self, depth_stencil_alpha: &PipeDepthStencilAlphaState) {
        unsafe { cso_set_depth_stencil_alpha(self.inner, &depth_stencil_alpha.0) };
    }

    pub fn set_rasterizer(&self, rasterizer: &PipeRasterizerState) {
        unsafe { cso_set_rasterizer(self.inner, &rasterizer.0) };
    }

    pub fn set_viewport(&self, viewport: &PipeViewportState) {
        unsafe { cso_set_viewport(self.inner, &viewport.0) };
    }

    pub fn set_vertex_shader_handle(&self, vs: *mut c_void) {
        unsafe { cso_set_vertex_shader_handle(self.inner, vs) };
    }

    pub fn set_fragment_shader_handle(&self, fs: *mut c_void) {
        unsafe { cso_set_fragment_shader_handle(self.inner, fs) };
    }

    pub fn set_samplers(&self, shader: pipe_shader_type, samplers: &[&PipeSamplerState]) {
        let mut samplers: Vec<_> = samplers
            .iter()
            .map(|sampler| &sampler.0 as *const _)
            .collect();
        unsafe {
            cso_set_samplers(
                self.inner,
                shader,
                samplers.len() as u32,
                samplers.as_mut_ptr(),
            )
        };
    }

    pub fn set_vertex_elements(&self, velem: &CsoVelemsState) {
        unsafe { cso_set_vertex_elements(self.inner, &velem.0) };
    }
}

pub struct PipeVertexElement(pipe_vertex_element);

impl PipeVertexElement {
    pub fn new(src_offset: u16, src_format: PipeFormat) -> PipeVertexElement {
        PipeVertexElement(pipe_vertex_element {
            src_offset,
            instance_divisor: 0,
            src_format: src_format as u8,
            _bitfield_align_1: Default::default(),
            _bitfield_1: gallium::pipe_vertex_element::new_bitfield_1(0, false),
        })
    }
}

#[derive(Debug)]
pub struct CsoVelemsState(cso_velems_state);

impl CsoVelemsState {
    pub fn new(elements: &[PipeVertexElement]) -> CsoVelemsState {
        let count = elements.len();
        let mut velems: [MaybeUninit<pipe_vertex_element>; 32] =
            unsafe { MaybeUninit::uninit().assume_init() };
        for i in 0..count {
            velems[i].write(elements[i].0);
        }
        CsoVelemsState(cso_velems_state {
            count: count as u32,
            velems: unsafe { std::mem::transmute(velems) },
        })
    }
}

#[derive(Debug)]
pub struct PipeFramebufferState(pipe_framebuffer_state);

impl PipeFramebufferState {
    pub fn empty() -> PipeFramebufferState {
        let cbufs = [null_mut(); 8];
        PipeFramebufferState(pipe_framebuffer_state {
            width: 0,
            height: 0,
            layers: 0,
            nr_cbufs: 0,
            cbufs,
            samples: 0,
            zsbuf: null_mut(),
        })
    }

    pub fn new(width: usize, height: usize, surfaces: &[&PipeSurface]) -> PipeFramebufferState {
        let mut cbufs = [null_mut(); 8];
        for (i, surface) in surfaces.iter().enumerate() {
            cbufs[i] = surface.inner;
        }
        PipeFramebufferState(pipe_framebuffer_state {
            width: width as u16,
            height: height as u16,
            layers: 0,
            nr_cbufs: surfaces.len() as u8,
            cbufs,
            samples: 0,
            zsbuf: null_mut(),
        })
    }

    pub fn add_depth_buffer(&mut self, zsbuf: &PipeSurface) -> &mut Self {
        println!("zsbuf={zsbuf:?}");
        self.0.zsbuf = zsbuf.inner;
        self
    }

    pub fn cbuf(&self, index: usize) -> Option<PipeSurface> {
        if index < self.0.nr_cbufs as usize {
            let inner = self.0.cbufs[index];
            Some(PipeSurface { inner })
        } else {
            None
        }
    }
}

#[derive(Debug)]
pub struct PipeBlendState(pipe_blend_state);

impl PipeBlendState {
    pub fn new() -> PipeBlendState {
        let mut rt: [pipe_rt_blend_state; 8] = (0..8)
            .map(|_| pipe_rt_blend_state {
                _bitfield_align_1: Default::default(),
                _bitfield_1: Default::default(),
            })
            .collect::<Vec<_>>()
            .try_into()
            .unwrap();
        rt[0].set_colormask(PIPE_MASK_RGBA);

        PipeBlendState(pipe_blend_state {
            rt,
            _bitfield_align_1: Default::default(),
            _bitfield_1: Default::default(),
        })
    }

    pub fn set_factors(
        &mut self,
        rgb_sf: pipe_blendfactor,
        rgb_df: pipe_blendfactor,
        alpha_sf: pipe_blendfactor,
        alpha_df: pipe_blendfactor,
    ) -> &mut Self {
        let rt = &mut self.0.rt[0];
        rt.set_blend_enable(1);
        rt.set_rgb_func(pipe_blend_func::PIPE_BLEND_ADD as u32);
        rt.set_rgb_src_factor(rgb_sf as u32);
        rt.set_rgb_dst_factor(rgb_df as u32);
        rt.set_alpha_func(pipe_blend_func::PIPE_BLEND_ADD as u32);
        rt.set_alpha_src_factor(alpha_sf as u32);
        rt.set_alpha_dst_factor(alpha_df as u32);
        self
    }
}

#[derive(Debug)]
pub struct PipeDepthStencilAlphaState(pipe_depth_stencil_alpha_state);

impl PipeDepthStencilAlphaState {
    pub fn new() -> PipeDepthStencilAlphaState {
        let stencil = [
            pipe_stencil_state {
                _bitfield_align_1: Default::default(),
                _bitfield_1: Default::default(),
            },
            pipe_stencil_state {
                _bitfield_align_1: Default::default(),
                _bitfield_1: Default::default(),
            },
        ];
        PipeDepthStencilAlphaState(pipe_depth_stencil_alpha_state {
            depth_bounds_min: f64::MIN,
            depth_bounds_max: f64::MAX,
            stencil,
            alpha_ref_value: Default::default(),
            _bitfield_align_1: Default::default(),
            _bitfield_1: Default::default(),
        })
    }

    pub fn set_alpha_ref_value(&mut self, value: f32) {
        self.0.alpha_ref_value = value;
    }

    pub fn set_alpha_func(&mut self, value: pipe_compare_func) {
        if value == pipe_compare_func::PIPE_FUNC_ALWAYS {
            if self.0.alpha_enabled() != 0 {
                self.0.set_alpha_enabled(0);
            }
        } else {
            self.0.set_alpha_enabled(1);
            self.0.set_alpha_func(value as u32);
        }
    }

    pub fn set_depth_func(&mut self, value: pipe_compare_func) {
        if value == pipe_compare_func::PIPE_FUNC_ALWAYS {
            if self.0.depth_enabled() != 0 {
                self.0.set_depth_enabled(0);
            }
        } else {
            self.0.set_depth_enabled(1);
            self.0.set_depth_func(value as u32);
        }
    }

    pub fn set_depth_writemask(&mut self, mask: bool) {
        self.0.set_depth_writemask(mask as u32);
    }
}

#[derive(Debug)]
pub struct PipeRasterizerState(pipe_rasterizer_state);

impl PipeRasterizerState {
    pub fn new(cull_face: u32) -> PipeRasterizerState {
        let mut inner = pipe_rasterizer_state {
            conservative_raster_dilate: Default::default(),
            line_width: 1.,
            offset_clamp: Default::default(),
            offset_scale: Default::default(),
            offset_units: Default::default(),
            point_size: 1.,
            sprite_coord_enable: Default::default(),
            _bitfield_align_1: Default::default(),
            _bitfield_1: Default::default(),
        };
        inner.set_cull_face(cull_face);
        inner.set_half_pixel_center(1);
        inner.set_bottom_edge_rule(1);
        inner.set_depth_clip_near(1);
        inner.set_depth_clip_far(1);
        PipeRasterizerState(inner)
    }
}

#[derive(Debug)]
pub struct PipeViewportState(pipe_viewport_state);

impl PipeViewportState {
    pub fn new(x: u32, y: u32, width: u32, height: u32, flip: bool) -> PipeViewportState {
        let x = x as f32;
        let y = y as f32;
        let z = 0.;
        let half_width = width as f32 / 2.;
        let half_height = height as f32 / 2.;
        let half_depth = 0.;
        let scale;
        let bias;
        if flip {
            scale = -1.;
            bias = height as f32;
        } else {
            scale = 1.;
            bias = 0.;
        }
        let scales = [half_width, half_height * scale, half_depth];
        let translate = [
            half_width + x,
            (half_height + y) * scale + bias,
            half_depth + z,
        ];
        PipeViewportState(pipe_viewport_state {
            scale: scales,
            translate,
            _bitfield_align_1: Default::default(),
            _bitfield_1: pipe_viewport_state::new_bitfield_1(
                pipe_viewport_swizzle::PIPE_VIEWPORT_SWIZZLE_POSITIVE_X,
                pipe_viewport_swizzle::PIPE_VIEWPORT_SWIZZLE_POSITIVE_Y,
                pipe_viewport_swizzle::PIPE_VIEWPORT_SWIZZLE_POSITIVE_Z,
                pipe_viewport_swizzle::PIPE_VIEWPORT_SWIZZLE_POSITIVE_W,
            ),
        })
    }
}

pub struct PipeSamplerState(pipe_sampler_state);

impl PipeSamplerState {
    pub fn new() -> PipeSamplerState {
        let f = [1., 1., 1., 1.];
        let border_color = pipe_color_union { f };
        let mut inner = pipe_sampler_state {
            border_color,
            lod_bias: Default::default(),
            min_lod: Default::default(),
            max_lod: Default::default(),
            _bitfield_align_1: Default::default(),
            _bitfield_1: Default::default(),
        };
        inner.set_wrap_s(pipe_tex_wrap::PIPE_TEX_WRAP_CLAMP_TO_EDGE as u32);
        inner.set_wrap_t(pipe_tex_wrap::PIPE_TEX_WRAP_CLAMP_TO_EDGE as u32);
        inner.set_wrap_r(pipe_tex_wrap::PIPE_TEX_WRAP_CLAMP_TO_EDGE as u32);
        inner.set_min_mip_filter(pipe_tex_mipfilter::PIPE_TEX_MIPFILTER_NONE as u32);
        inner.set_min_img_filter(pipe_tex_mipfilter::PIPE_TEX_MIPFILTER_NEAREST as u32);
        inner.set_mag_img_filter(pipe_tex_mipfilter::PIPE_TEX_MIPFILTER_NEAREST as u32);
        inner.set_normalized_coords(1);
        PipeSamplerState(inner)
    }
}

pub enum PipeUsage {
    Default,
    Immutable,
    Dynamic,
    Stream,
    Staging,
}

impl From<PipeUsage> for pipe_resource_usage {
    fn from(usage: PipeUsage) -> pipe_resource_usage {
        match usage {
            PipeUsage::Default => pipe_resource_usage::PIPE_USAGE_DEFAULT,
            PipeUsage::Immutable => pipe_resource_usage::PIPE_USAGE_IMMUTABLE,
            PipeUsage::Dynamic => pipe_resource_usage::PIPE_USAGE_DYNAMIC,
            PipeUsage::Stream => pipe_resource_usage::PIPE_USAGE_STREAM,
            PipeUsage::Staging => pipe_resource_usage::PIPE_USAGE_STAGING,
        }
    }
}

pub enum PipePrim {
    Points,
    Lines,
    Triangles,
}

impl From<PipePrim> for pipe_prim_type {
    fn from(prim: PipePrim) -> pipe_prim_type {
        match prim {
            PipePrim::Points => pipe_prim_type::PIPE_PRIM_POINTS,
            PipePrim::Lines => pipe_prim_type::PIPE_PRIM_LINES,
            PipePrim::Triangles => pipe_prim_type::PIPE_PRIM_TRIANGLES,
        }
    }
}

pub enum PipeClear {
    Color,
    Depth,
    Stencil,
}

pub struct PipeColorUnion {
    inner: pipe_color_union,
}

impl PipeColorUnion {
    pub fn new_f(r: f32, g: f32, b: f32, a: f32) -> PipeColorUnion {
        let f = [r, g, b, a];
        PipeColorUnion {
            inner: pipe_color_union { f },
        }
    }
}

pub struct PipeBox(pipe_box);

impl PipeBox {
    pub fn new_2d(x: usize, y: usize, width: usize, height: usize) -> PipeBox {
        PipeBox(pipe_box {
            x: x as i32,
            y: y as i16,
            z: 0,
            width: width as i32,
            height: height as i16,
            depth: 1,
        })
    }
}

pub struct PipeSamplerViewTemplate(pipe_sampler_view);

impl PipeSamplerViewTemplate {
    pub fn new() -> PipeSamplerViewTemplate {
        use gallium::{
            pipe_sampler_view__bindgen_ty_1, pipe_sampler_view__bindgen_ty_2,
            pipe_sampler_view__bindgen_ty_2__bindgen_ty_2,
        };
        let u = pipe_sampler_view__bindgen_ty_2 {
            buf: pipe_sampler_view__bindgen_ty_2__bindgen_ty_2 { offset: 0, size: 0 },
        };
        PipeSamplerViewTemplate(pipe_sampler_view {
            context: null_mut(),
            texture: null_mut(),
            u,
            __bindgen_anon_1: pipe_sampler_view__bindgen_ty_1 {
                reference: pipe_reference { count: 0 },
            },
            _bitfield_align_1: Default::default(),
            _bitfield_1: Default::default(),
        })
    }
}

pub struct PipeSamplerView {
    inner: *mut pipe_sampler_view,
}

pub struct PipeConstantBuffer(pipe_constant_buffer);

impl PipeConstantBuffer {
    pub fn from<T>(data: &[T]) -> PipeConstantBuffer {
        PipeConstantBuffer(pipe_constant_buffer {
            buffer: null_mut(),
            buffer_offset: 0,
            buffer_size: (data.len() * std::mem::size_of::<T>()) as u32,
            user_buffer: data.as_ptr() as *const c_void,
        })
    }
}

pub mod util {
    use super::{PipeContext, PipeFormat, PipeResource, PipeSamplerViewTemplate};
    use crate::gallium::{
        tgsi_interpolate_mode, tgsi_return_type, tgsi_semantic, tgsi_texture_type,
        u_sampler_view_default_template, util_make_fragment_passthrough_shader,
        util_make_fragment_tex_shader, util_make_vertex_passthrough_shader,
    };
    use std::os::raw::c_void;

    pub fn make_vertex_passthrough_shader<const NUM_ATTRIBS: usize>(
        pipe: &PipeContext,
        semantic_names: [tgsi_semantic; NUM_ATTRIBS],
        semantic_indices: [u32; NUM_ATTRIBS],
        window_space: bool,
    ) -> *mut c_void {
        unsafe {
            util_make_vertex_passthrough_shader(
                pipe.inner,
                NUM_ATTRIBS as u32,
                semantic_names.as_ptr(),
                semantic_indices.as_ptr(),
                window_space,
            )
        }
    }

    pub fn make_fragment_passthrough_shader(
        pipe: &PipeContext,
        input_semantic: tgsi_semantic,
        input_interpolate: tgsi_interpolate_mode,
        write_all_cbufs: bool,
    ) -> *mut c_void {
        unsafe {
            util_make_fragment_passthrough_shader(
                pipe.inner,
                input_semantic as i32,
                input_interpolate as i32,
                if write_all_cbufs { 1 } else { 0 },
            )
        }
    }

    pub fn make_fragment_tex_shader(
        pipe: &PipeContext,
        input_interpolate: tgsi_interpolate_mode,
    ) -> *mut c_void {
        unsafe {
            util_make_fragment_tex_shader(
                pipe.inner,
                tgsi_texture_type::TGSI_TEXTURE_2D,
                input_interpolate,
                tgsi_return_type::TGSI_RETURN_TYPE_FLOAT,
                tgsi_return_type::TGSI_RETURN_TYPE_FLOAT,
                false,
                false,
            )
        }
    }

    pub fn sampler_view_default_template(
        view: &mut PipeSamplerViewTemplate,
        texture: &PipeResource,
        format: PipeFormat,
    ) {
        unsafe { u_sampler_view_default_template(&mut view.0, texture.inner, format) }
    }
}

pub mod debug {
    use super::{PipeContext, PipeFramebufferState, TextureMap};
    use gallium::{debug_dump_surface_bmp, debug_dump_transfer_bmp};
    use std::ffi::CString;

    pub fn dump_surface_bmp<F: AsRef<str>>(
        pipe: &PipeContext,
        filename: F,
        framebuffer: &PipeFramebufferState,
    ) {
        if let Ok(filename) = CString::new(filename.as_ref()) {
            unsafe {
                debug_dump_surface_bmp(pipe.inner, filename.as_ptr(), framebuffer.0.cbufs[0]);
                //debug_dump_surface_bmp(pipe.inner, filename.as_ptr(), framebuffer.0.zsbuf);
            }
        }
    }

    pub fn dump_transfer_bmp<F: AsRef<str>>(pipe: &PipeContext, filename: F, texture: &TextureMap) {
        if let Ok(filename) = CString::new(filename.as_ref()) {
            unsafe {
                debug_dump_transfer_bmp(pipe.inner, filename.as_ptr(), texture.t, texture.ptr)
            };
        }
    }
}

pub mod tgsi {
    use gallium::{tgsi_text_translate, tgsi_token};

    pub fn text_translate(text: &str) -> Result<Vec<tgsi_token>, ()> {
        let mut tokens = Vec::with_capacity(1024);
        let ret =
            unsafe { tgsi_text_translate(text.as_ptr() as *const i8, tokens.as_mut_ptr(), 1024) };
        if ret == 0 {
            return Err(());
        }
        unsafe { tokens.set_len(1024) };
        Ok(tokens)
    }
}

use gallium::{
    pipe_shader_ir, pipe_shader_state, pipe_shader_state__bindgen_ty_1, pipe_stream_output,
    pipe_stream_output_info, tgsi_token,
};

pub fn shader_state_from_tgsi(tokens: &[tgsi_token]) -> pipe_shader_state {
    let tokens = tokens.as_ptr();
    pipe_shader_state {
        type_: pipe_shader_ir::PIPE_SHADER_IR_TGSI,
        tokens,
        stream_output: pipe_stream_output_info {
            num_outputs: 0,
            output: [pipe_stream_output {
                _bitfield_align_1: Default::default(),
                _bitfield_1: pipe_stream_output::new_bitfield_1(0, 0, 0, 0, 0, 0),
            }; 64],
            stride: [0u16; 4],
        },
        ir: pipe_shader_state__bindgen_ty_1 { native: null_mut() },
    }
}
