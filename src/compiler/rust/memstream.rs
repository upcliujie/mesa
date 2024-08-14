// Copyright © 2024 Collabora, Ltd.
// SPDX-License-Identifier: MIT

use std::io;
use std::marker::PhantomPinned;
use std::pin::Pin;

use crate::bindings;

struct MemStreamImpl {
    stream: bindings::u_memstream,
    buffer: *mut u8,
    buffer_size: usize,
    _pin: PhantomPinned,
}

/// A Rust memstream abstraction. Useful when interacting with C code that
/// expects a FILE* pointer.
///
/// The size of the buffer is managed by the C code automatically.
pub struct MemStream(Pin<Box<MemStreamImpl>>);

impl MemStream {
    pub fn new() -> io::Result<Self> {
        let mut stream_impl = Box::pin(MemStreamImpl {
            stream: bindings::u_memstream {
                f: std::ptr::null_mut(),
            },
            buffer: std::ptr::null_mut(),
            buffer_size: 0,
            _pin: PhantomPinned,
        });

        unsafe {
            let stream_impl = stream_impl.as_mut().get_unchecked_mut();
            if !bindings::u_memstream_open(
                &mut stream_impl.stream,
                &mut stream_impl.buffer as *mut *mut u8 as *mut *mut i8,
                &mut stream_impl.buffer_size,
            ) {
                return Err(io::Error::last_os_error());
            }
        }

        Ok(Self(stream_impl))
    }

    // Safety: caller must ensure that inner is not moved through the returned
    // reference.
    unsafe fn inner_mut(&mut self) -> &mut MemStreamImpl {
        unsafe { self.0.as_mut().get_unchecked_mut() }
    }

    /// Flushes the stream so written data appears in the stream
    pub fn flush(&mut self) -> io::Result<()> {
        unsafe {
            let stream = self.inner_mut();
            if bindings::u_memstream_flush(&mut stream.stream) != 0 {
                return Err(io::Error::last_os_error());
            }
        }

        Ok(())
    }

    /// Resets the MemStream
    pub fn reset(&mut self) -> io::Result<()> {
        unsafe {
            let stream = self.inner_mut();
            if bindings::u_memstream_reset(&mut stream.stream) != 0 {
                return Err(io::Error::last_os_error());
            }
        }

        Ok(())
    }

    /// Resets the MemStream and returns its contents
    pub fn take(&mut self) -> io::Result<Vec<u8>> {
        self.flush()?;
        let mut vec = Vec::new();
        vec.extend_from_slice(self.as_ref());
        self.reset()?;
        Ok(vec)
    }

    /// Resets the MemStream and returns its contents as a UTF-8 string
    pub fn take_utf8_string_lossy(&mut self) -> io::Result<String> {
        self.flush()?;
        let string = String::from_utf8_lossy(self.as_ref()).into_owned();
        self.reset()?;
        Ok(string)
    }

    /// Returns the current position in the stream.
    pub fn position(&self) -> usize {
        unsafe { bindings::compiler_rs_ftell(self.c_file()) as usize }
    }

    /// Seek to a position relative to the start of the stream.
    pub fn seek(&mut self, offset: i64) -> io::Result<()> {
        unsafe {
            if bindings::compiler_rs_fseek(self.c_file(), offset, 0) != 0 {
                Err(io::Error::last_os_error())
            } else {
                Ok(())
            }
        }
    }

    /// Returns the underlying C file.
    ///
    /// # Safety
    ///
    /// The memstream abstraction assumes that the file is valid throughout its
    /// lifetime.
    pub unsafe fn c_file(&self) -> *mut bindings::FILE {
        self.0.stream.f
    }
}

impl AsRef<[u8]> for MemStream {
    /// Returns a slice view into the memstream
    ///
    /// This is only safe with respect to other safe Rust methods.  Even though
    /// this takes a reference to the stream there is nothing preventing you
    /// from modifying the stream through the FILE with unsafe C code.
    fn as_ref(&self) -> &[u8] {
        // SAFETY: this does not move the stream and we know that
        // self.position() cannot exceed the stream size as per the
        // open_memstream() API.
        unsafe { std::slice::from_raw_parts(self.0.buffer, self.position()) }
    }
}

impl Drop for MemStream {
    fn drop(&mut self) {
        // SAFETY: this does not move the stream.
        unsafe {
            bindings::u_memstream_close(&mut self.inner_mut().stream);
            bindings::compiler_rs_free(self.0.buffer as *mut std::ffi::c_void);
        }
    }
}
