#![no_std]
#![crate_type = "rlib"] 

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

#[panic_handler]
fn my_panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
