#![no_std]
#![crate_type = "staticlib"]
#![allow(non_camel_case_types)]

use core::cmp::min;
use core::mem::size_of;
use core::slice::{from_raw_parts, from_raw_parts_mut};
use core::sync::atomic::AtomicBool;
use core::sync::atomic::Ordering;
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

#[derive(Copy, Clone)]
struct StaticArray<T: 'static>(&'static [T], &'static [T]);

impl<T> StaticArray<T> {
    pub fn len(&self) -> usize {
        let Self(start, end) = self;
        (isize::wrapping_sub(end.as_ptr() as _, start.as_ptr() as _) / size_of::<T>() as isize)
            as usize
    }

    pub fn as_slice(&self) -> &'static [T] {
        let Self(start, _) = self;
        // Safe assuming the linker gave us valid addresses.
        unsafe { from_raw_parts(start.as_ptr(), self.len()) }
    }
}

struct StaticMutArray<T: 'static>(&'static mut [T], &'static mut [T]);

impl<T> StaticMutArray<T> {
    pub fn len(&self) -> usize {
        let Self(start, end) = self;
        (isize::wrapping_sub(end.as_ptr() as _, start.as_ptr() as _) / size_of::<T>() as isize)
            as usize
    }

    pub fn as_slice(&self) -> &'static [T] {
        let Self(start, _) = self;
        // Safe assuming the linker gave us valid addresses.
        unsafe { from_raw_parts(start.as_ptr(), self.len()) }
    }

    pub fn as_mut_slice(&mut self) -> &mut [T] {
        let Self(start, _) = self;
        // Safe assuming the linker gave us valid addresses.
        unsafe { from_raw_parts_mut(start.as_mut_ptr(), self.len()) }
    }
}

static mut DEFERRED_UNTIL: StaticMutArray<u64> =
    unsafe { StaticMutArray(&mut __deferred_until, &mut __deferred_until_end) };
static mut DEFERRED_FUNCS: StaticArray<deferred_data> =
    unsafe { StaticArray(&__deferred_funcs, &__deferred_funcs_end) };

#[no_mangle]
pub fn hook_notify(type_: hook_type) {
    if type_ as usize >= HOOK_LIST.len() {
        return;
    }

    let hooks = HOOK_LIST[type_ as usize].as_slice();

    let mut count = 0;
    let mut last_priority = (HOOK_PRIO_FIRST - 1) as c_int;
    // Call all the hooks in priority order.
    while count < hooks.len() {
        // Find the lowest remaining priority
        let mut min_priority = (HOOK_PRIO_LAST + 1) as c_int;
        for hook in hooks.iter() {
            if hook.priority < min_priority && hook.priority > last_priority {
                min_priority = hook.priority
            }
        }
        last_priority = min_priority;

        // Call all the hooks with that priority
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

static HOOK_TASK_STARTED: AtomicBool = AtomicBool::new(false);
static DEFER_NEW_CALL: AtomicBool = AtomicBool::new(false);

fn system_duration() -> Duration {
    Duration::from_micros(unsafe { get_time().val })
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
        // Cancel
        *p = 0;
    } else {
        let future = system_duration().as_micros() as u64;
        // Set Alarm
        *p = future + us as u64;
        // Flag that hook_call_deferred() has been called.  If the hook task is already active, this
        // will allow it to go through the loop one more time before sleeping.
        DEFER_NEW_CALL.store(true, Ordering::SeqCst);

        if HOOK_TASK_STARTED.load(Ordering::SeqCst) {
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

#[no_mangle]
pub extern "C" fn hook_task(_u: c_void) -> ! {
    const HOOK_TICK_INTERVAL: Duration = Duration::from_millis(HOOK_TICK_INTERVAL_MS as u64);
    const SECOND: Duration = Duration::from_secs(1);
    const NO_TIME: Duration = Duration::from_secs(0);
    unsafe_static_mut!(static mut last_second: Duration = HOOK_TICK_INTERVAL);
    unsafe_static_mut!(static mut last_tick: Duration = HOOK_TICK_INTERVAL);

    HOOK_TASK_STARTED.store(true, Ordering::SeqCst);

    // Call HOOK_INIT hooks
    hook_notify(HOOK_INIT);
    // Now, enable the rest of the tasks.
    unsafe {
        task_enable_all_tasks();
    }

    loop {
        let t = system_duration();
        let mut next = Duration::from_secs(0);

        for (deferred_until, deferred_func) in unsafe {
            DEFERRED_UNTIL
                .as_mut_slice()
                .iter_mut()
                .zip(DEFERRED_FUNCS.as_slice().iter())
        } {
            if *deferred_until != 0 && Duration::from_micros(*deferred_until) < t {
                // Call deferred function.  Clear timer first, so it can request itself be called
                // later.
                *deferred_until = 0;
                if let Some(routine) = deferred_func.routine {
                    // The hook's routine is always unsafe to call.
                    unsafe {
                        routine();
                    }
                }
            }
        }

        if duration_elapsed(t, *last_tick, HOOK_TICK_INTERVAL) {
            hook_notify(HOOK_TICK);
            *last_tick = t;
        }

        if duration_elapsed(t, *last_second, SECOND) {
            hook_notify(HOOK_SECOND);
            *last_second = t;
        }

        // Calculate when next tick needs to occur
        let t = system_duration();
        if *last_tick + HOOK_TICK_INTERVAL > t {
            next = *last_tick + HOOK_TICK_INTERVAL - t;
        }

        // Wake earlier if needed by a deferred routine
        DEFER_NEW_CALL.store(false, Ordering::SeqCst);

        for deferred_until in unsafe { DEFERRED_UNTIL.as_slice().iter() } {
            if *deferred_until == 0 {
                continue;
            }

            let future_duration = Duration::from_micros(*deferred_until)
                .checked_sub(t)
                .unwrap_or(NO_TIME);
            next = min(future_duration, next)
        }

        // If nothing is immediately pending, and hook_call_deferred() hasn't been called since we
        // started calculating next, sleep until the next event.
        if next > NO_TIME && !DEFER_NEW_CALL.load(Ordering::SeqCst) {
            unsafe { task_wait_event(next.as_micros() as i32) };
        }
    }
}
