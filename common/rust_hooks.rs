#![no_std]
#![crate_type = "staticlib"]
#![allow(non_camel_case_types)]

extern crate common;

use core::mem::size_of;
use core::slice::from_raw_parts;
use core::time::Duration;

use common::generated::*;
use common::raw::*;

type HookEndpint = &'static [hook_data];
#[derive(Copy, Clone)]
struct HookInterval(HookEndpint, HookEndpint);

impl HookInterval {
    fn len(&self) -> usize {
        let &HookInterval(start, end) = self;
        (isize::wrapping_sub(end.as_ptr() as _, start.as_ptr() as _)
            / size_of::<hook_data>() as isize) as usize
    }

    fn as_slice(&self) -> &'static [hook_data] {
        let &HookInterval(start, _) = self;
        // Safe assuming the linker gave us valid addresses.
        unsafe { from_raw_parts(start.as_ptr(), self.len()) }
    }
}

static HOOK_LIST: &[HookInterval] = unsafe {
    &[
        HookInterval(&__hooks_init, &__hooks_init_end),
        HookInterval(&__hooks_pre_freq_change, &__hooks_pre_freq_change_end),
        HookInterval(&__hooks_freq_change, &__hooks_freq_change_end),
        HookInterval(&__hooks_sysjump, &__hooks_sysjump_end),
        HookInterval(&__hooks_chipset_pre_init, &__hooks_chipset_pre_init_end),
        HookInterval(&__hooks_chipset_startup, &__hooks_chipset_startup_end),
        HookInterval(&__hooks_chipset_resume, &__hooks_chipset_resume_end),
        HookInterval(&__hooks_chipset_suspend, &__hooks_chipset_suspend_end),
        HookInterval(&__hooks_chipset_shutdown, &__hooks_chipset_shutdown_end),
        HookInterval(&__hooks_chipset_reset, &__hooks_chipset_reset_end),
        HookInterval(&__hooks_ac_change, &__hooks_ac_change_end),
        HookInterval(&__hooks_lid_change, &__hooks_lid_change_end),
        HookInterval(&__hooks_tablet_mode_change, &__hooks_tablet_mode_change_end),
        HookInterval(
            &__hooks_base_attached_change,
            &__hooks_base_attached_change_end,
        ),
        HookInterval(&__hooks_pwrbtn_change, &__hooks_pwrbtn_change_end),
        HookInterval(&__hooks_battery_soc_change, &__hooks_battery_soc_change_end),
        #[cfg(feature = "CONFIG_CASE_CLOSED_DEBUG_V1")]
        HookInterval(&__hooks_ccd_change, &__hooks_ccd_change_end),
        #[cfg(feature = "CONFIG_USB_SUSPEND")]
        HookInterval(&__hooks_usb_change, &__hooks_usb_change_end),
        HookInterval(&__hooks_tick, &__hooks_tick_end),
        HookInterval(&__hooks_second, &__hooks_second_end),
        HookInterval(&__hooks_usb_pd_disconnect, &__hooks_usb_pd_disconnect_end),
    ]
};

#[no_mangle]
pub fn hook_notify(type_: hook_type) {
    if type_ as usize >= HOOK_LIST.len() {
        return;
    }

    let hooks = HOOK_LIST[type_ as usize].as_slice();

    let mut count = 0;
    let mut last_priority = (HOOK_PRIO_FIRST - 1) as c_int;
    while count < hooks.len() {
        let mut min_priority = (HOOK_PRIO_LAST + 1) as c_int;
        for hook in hooks.iter() {
            if hook.priority < min_priority && hook.priority > last_priority {
                min_priority = hook.priority
            }
        }
        last_priority = min_priority;
        for hook in hooks.iter() {
            if hook.priority == min_priority {
                count += 1;
                if let Some(routine) = hook.routine {
                    // The hook's routine is always unsafe to call.
                    unsafe {
                        routine();
                    }
                }
            }
        }
    }
}

extern "C" {
    static mut defer_new_call: i32;
    static mut hook_task_started: i32;
}

fn system_duration() -> Duration {
    Duration::from_micros(unsafe {
        get_time().val
    })
}

fn duration_since(d: Duration) -> Duration {
    system_duration() - d
}

fn duration_elapsed(now: Duration, then: Duration, elapsed: Duration) -> bool {
    if let Some(diff) = now.checked_sub(then) {
        diff > elapsed
    } else {
        false
    }
}

#[no_mangle]
pub unsafe extern "C" fn hook_call_deferred(data: *const deferred_data, us: i32) -> c_int {
    let i = isize::wrapping_sub(data as _, __deferred_funcs.as_ptr() as _)
        / size_of::<deferred_data>() as isize;

    if data < __deferred_funcs.as_ptr() || data >= __deferred_funcs_end.as_ptr() {
        return EC_ERROR_INVAL as c_int;
    }

    let p: &mut u64 = __deferred_until.get_unchecked_mut(i as usize);
    if us < 0 {
        *p = 0;
    } else {
        let future = system_duration().as_micros() as u64;
        *p = future + us as u64;
        defer_new_call = 1;

        if hook_task_started != 0 {
            task_set_event(TASK_ID_HOOKS as task_id_t, TASK_EVENT_WAKE, 0);
        }
    }

    EC_SUCCESS as c_int
}

macro_rules! unsafe_static_mut {
    (static mut $id:ident: $t:ty = $init:expr) => {
        let $id = unsafe {
            static mut $id: $t = $init;
            &mut $id
        };
    };
}

// TODO(zachr): WIP
#[no_mangle]
pub extern "C" fn hook_task(u: c_void) -> ! {
    // let last_second: &mut u64 = unsafe {
    //     static mut last_second: u64 = 0;
    //     &mut last_second
    // };
    const HOOK_TICK_INTERVAL: Duration = Duration::from_millis(HOOK_TICK_INTERVAL_MS as u64);
    unsafe_static_mut!(static mut last_second: Duration = HOOK_TICK_INTERVAL);
    unsafe_static_mut!(static mut last_tick: Duration = HOOK_TICK_INTERVAL);

    hook_notify(HOOK_INIT);
    unsafe {
        task_enable_all_tasks();
    }

    loop {
        let t = system_duration();
        let next = Duration::from_secs(0);

        if duration_elapsed(t, *last_tick, HOOK_TICK_INTERVAL)  {
            hook_notify(HOOK_TICK);
            *last_tick = t;
        }

        if duration_elapsed(t, *last_second, Duration::from_secs(1)) {
            hook_notify(HOOK_SECOND);
            *last_second = t;
        }

        let t = system_duration();
        if *last_tick + HOOK_TICK_INTERVAL > t {
            let next = *last_tick + HOOK_TICK_INTERVAL - t;
        }

        unsafe { defer_new_call = 0 }
    

        if next > Duration::from_secs(0) && unsafe { defer_new_call == 0 } {
            unsafe {
                task_wait_event(next.as_micros() as i32)
            };
        }
    }
}