# OPtions:
# BOARD=
# SERVER=segger|openocd

define ec-connect
	#target remote :3333
	target extended-remote :3333
end
alias connect = ec-connect

connect

python
import os
board = os.getenv('BOARD', 'nami_fpv2')
build = 'build/' + board

if not os.path.isdir(build):
	print('Error - Build path "' + build + '" doesn\'t exist. Aborting.')
	gdb.execute('quit')

gdb.execute('file ' + build + '/ec.obj')
gdb.execute('add-symbol-file ' + build + '/RO/ec.RO.elf')
gdb.execute('add-symbol-file ' + build + '/RW/ec.RW.elf')

gdb.execute('set $server_openocd = 0')
gdb.execute('set $server_segger = 1')
if os.getenv('SERVER', 'openocd') == 'segger':
	gdb.execute('set $server = 1')
	gdb.execute('monitor flash breakpoints = 0')
else:
	gdb.execute('set $server = 0')
end

if $server == $server_openocd
	# don't automatically choose hw or software breakpints based on memory-map
	set breakpoint auto-hw off
end
# forces breakpoints to be inserted at all times -- won't reinsert on reconnect
#set breakpoint always-inserted on

#break exception_panic
#break flash_physical_erase

define ec-exception
	break exception_panic
	break bus_fault_handler
	break panic_assert_fail
end

define ec-reset
	if $server == $server_openocd
		monitor halt
		monitor reset halt
	else
		monitor halt
		monitor reset
	end
end
alias reset = ec-reset

define ec-tasks
	set $taskid_cur = current_task - tasks
	set $id = 0
	set $taskcount = sizeof(tasks)/sizeof(tasks[0])

	printf "Task Ready Name         Events      Time (s)  StkUsed\n"
	while $id < $taskcount

		set $is_ready = (tasks_ready & (1<<$id)) ? 'R' : ' '
		set $unused = (uint32_t)0xdeadd00d
		set $stacksize = tasks_init[$id].stack_size
		set $stackused = $stacksize
		set $s = tasks[$id].stack
		while $s < (uint32_t *)tasks[$id].sp && *$s == $unused
			set $stackused -= sizeof(uint32_t)
			set $s++
		end

		printf "%4d %-5c %-16s %08x %11.6ld  %3d/%3d", $id, $is_ready,  task_names[$id], tasks[$id].events, tasks[$id].runtime, $stackused, tasks_init[$id].stack_size
		if $stackused == $stacksize
			printf "\t [overrun]"
		end
		if $id == $taskid_cur
			printf "\t*"
		end
		printf "\n"
		set $id++
	end
	if $taskid_cur == scratchpad
		printf "Current task is set to scratchpad\n"
	end
end

define ec-stayro
	break common/rwsig.c:282
	commands
		set evt = 1
		printf "Continuing as RO\n"
		continue
	end
end

set $flashbase = 0x52002000
set $flashsr = $flashbase + 0x10

define reg32
	set $a = $arg0
	if $argc > 1
		set $a += $arg1
	end
	set $v = *((uint32_t *)$a)
	print $v
	print /x $v
	print /t $v
end

# Decode Excpetion
define armex
  printf "EXEC_RETURN (LR):\n",
  info registers $lr
    if ($lr & (0x4 == 0x4))
      printf "Uses MSP 0x%x return.\n", $msp
      set $armex_base = (uint32_t *) $msp
    else
      printf "Uses PSP 0x%x return.\n", $psp
      set $armex_base = (uint32_t *) $psp
    end

    printf "xPSR            0x%x\n", *($armex_base+7)
    printf "ReturnAddress   0x%x\n", *($armex_base+6)
    printf "LR (R14)        0x%x\n", *($armex_base+5)
    printf "R12             0x%x\n", *($armex_base+4)
    printf "R3              0x%x\n", *($armex_base+3)
    printf "R2              0x%x\n", *($armex_base+2)
    printf "R1              0x%x\n", *($armex_base+1)
    printf "R0              0x%x\n", *($armex_base+0)
    printf "Return instruction:\n"
    x/i *($armex_base+6)
    printf "LR instruction:\n"
    x/i *($armex_base+5)
end

document armex
ARMv7 Exception entry behavior.
xPSR, ReturnAddress, LR (R14), R12, R3, R2, R1, and R0
end
