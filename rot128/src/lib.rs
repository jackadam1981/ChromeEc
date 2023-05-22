#![crate_type = "staticlib"]
#![no_main]
#![no_std]

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_panic: &PanicInfo<'_>) -> ! {
    loop {}
}

#[no_mangle]
pub extern "C" fn rot128_decode(buffer: *mut u8, size: usize) -> bool {
    unsafe { internal::rot128_decode(buffer, size) }
}

extern "C" {
    fn wrapping_add(num: u8, val: u8) -> u8;
}

mod internal {
    use crate::wrapping_add;

    pub unsafe fn rot128_decode(buffer: *mut u8, size: usize) -> bool {
        let mid = size / 2;
        for i in 0..mid {
            unsafe {
                *buffer.offset(i as isize) = wrapping_add(*buffer.offset(i as isize), 128);
            }
        }
        for i in mid..size {
            unsafe {
                *buffer.offset(i as isize) = (*buffer.offset(i as isize)).wrapping_sub(128);
            }
        }

        true
    }
}
