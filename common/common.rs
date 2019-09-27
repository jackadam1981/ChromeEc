#![no_std]
#![crate_type = "rlib"] 

use core::mem::size_of;
use core::ops::{Add, Sub};
use core::slice::{from_raw_parts, from_raw_parts_mut};

#[allow(non_camel_case_types)]
pub mod raw {
    pub type c_void = core::ffi::c_void;
    pub type c_char = u8;
    pub type c_schar = i8;
    pub type c_uchar = u8;
    pub type c_short = i16;
    pub type c_ushort = u16;
    pub type c_int = i32;
    pub type c_uint = u32;
    pub type c_long = i32;
    pub type c_ulong = u32;
}


#[allow(non_camel_case_types)]
#[allow(non_upper_case_globals)]
pub mod generated {
    include! {
        concat!("../", env!("OUT_DIR"), "/generated.rs")
    }
}

// An accessor to a constant static array represented by the beginning and ending pointer.
pub struct StaticArray<T: 'static>(pub &'static [T; 0], pub &'static [T; 0]);

impl<T> StaticArray<T> {
    #[inline]
    pub fn len(&self) -> usize {
        let Self(start, end) = self;
        (isize::wrapping_sub(end.as_ptr() as _, start.as_ptr() as _) / size_of::<T>() as isize)
            as usize
    }

    #[inline]
    pub unsafe fn as_slice(&self) -> &'static [T] {
        let Self(start, _) = self;
        from_raw_parts(start.as_ptr() as _, self.len())
    }
}

// An accessor to a mutable static array represented by the beginning and ending pointer.
pub struct StaticMutArray<T: 'static>(pub &'static mut [T; 0], pub &'static mut [T; 0]);

impl<T> StaticMutArray<T> {
    #[inline]
    pub fn len(&self) -> usize {
        let Self(start, end) = self;
        (isize::wrapping_sub(end.as_ptr() as _, start.as_ptr() as _) / size_of::<T>() as isize)
            as usize
    }

    #[inline]
    pub unsafe fn as_slice(&self) -> &'static [T] {
        let Self(start, _) = self;
        from_raw_parts(start.as_ptr() as _, self.len())
    }

    #[inline]
    pub unsafe fn as_mut_slice(&mut self) -> &mut [T] {
        let Self(start, _) = self;
        from_raw_parts_mut(start.as_ptr() as _, self.len())
    }
}

/// An instant in time as measured in microseconds by the EC. Similar to `std::time::Instant`.
#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct EcInstant(pub u64);
impl EcInstant {
    #[inline]
    pub fn now() -> EcInstant {
        EcInstant(unsafe { generated::get_time().val })
    }

    #[inline]
    pub fn as_micros(&self) -> u64 {
        self.0
    }


    #[inline]
    pub fn checked_duration_since(&self, earlier: EcInstant) -> Option<EcDuration> {
        Some(EcDuration(self.0.checked_sub(earlier.0)?))
    }

    #[inline]
    pub fn duration_since_elapsed(&self, earlier: EcInstant, elapsed: EcDuration) -> bool {
        if let Some(since_earlier) = self.checked_duration_since(earlier) {
            since_earlier >= elapsed
        } else {
            false
        }
    }
}

impl Add<EcDuration> for EcInstant {
    type Output = Self;

    fn add(self, other: EcDuration) -> Self {
        Self(self.0 + other.0)
    }
}

/// A magnitude of time measured in microseconds. Similar to `std::time::Duration`.
#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct EcDuration(pub u64);
impl EcDuration {
    #[inline]
    pub fn from_millis(millis: u64) -> EcDuration {
        EcDuration(millis * 1000)
    }

    #[inline]
    pub fn as_micros(&self) -> u64 {
        self.0
    }


    #[inline]
    pub fn zero(&self) -> bool {
        self.0 == 0
    }
}

impl Add for EcDuration {
    type Output = Self;

    fn add(self, other: Self) -> Self {
        Self(self.0 + other.0)
    }
}

impl Sub for EcDuration {
    type Output = Self;

    fn sub(self, other: Self) -> Self {
        Self(self.0 - other.0)
    }
}

pub const SECOND: EcDuration = EcDuration(1_000_000);

/// Puts a string into the given console channel.
pub fn str_put(channel: generated::console_channel, s: &str) {
    let mut cbuf = [0, 0];
    for c in s.bytes() {
        cbuf[0] = c as raw::c_char;
        // Safe because the c string given is always 2 bytes, with the second byte being a null
        // terminator.
        unsafe {
            generated::cputs(channel, &cbuf[0]);
        }
    }
}


#[panic_handler]
fn my_panic(info: &core::panic::PanicInfo) -> ! {
    use generated::{CC_SYSTEM, usleep, cprintf, cflush};
    // Print the file name and line number of the panic.
    unsafe {
        str_put(CC_SYSTEM, "rust panic @ ");
        if let Some(loc) = info.location() {
            str_put(CC_SYSTEM, loc.file());
            cprintf(CC_SYSTEM, ":%d\0".as_ptr(), loc.line());
        } else {
            str_put(CC_SYSTEM, "<unknown>");
        }
        if let Some(payload) = info.payload().downcast_ref::<&str>() {
           str_put(CC_SYSTEM, " `");
           str_put(CC_SYSTEM, payload);
           str_put(CC_SYSTEM, "`");
        }
        str_put(CC_SYSTEM, "\n");
        cflush();
    }
    // The panic handler can not return, so we loop forever.
    loop {
        unsafe {
            usleep(10_000_000)
        }
    }
}
