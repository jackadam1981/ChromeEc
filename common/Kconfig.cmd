
config CMD_ACCEL_FIFO
	bool "CMD_ACCEL_FIFO"
	default n

config CMD_ACCEL_INFO
	bool "CMD_ACCEL_INFO"
	default n

config CMD_ACCELS
	bool "CMD_ACCELS"
	default n

config CMD_ACCELSPOOF
	bool "CMD_ACCELSPOOF"
	default y

config CMD_ADC
	bool "CMD_ADC"
	depends on ADC
	default y

config CMD_ALS
	bool "CMD_ALS"
	default n

config CMD_AP_RESET_LOG
	bool "CMD_AP_RESET_LOG"
	default n

config CMD_APTHROTTLE
	bool "CMD_APTHROTTLE"
	default y

config CMD_BATDEBUG
	bool "CMD_BATDEBUG"
	default n

config CMD_BATTFAKE
	bool "CMD_BATTFAKE"
	default y

config CMD_BATT_MFG_ACCESS
	bool "CMD_BATT_MFG_ACCESS"
	default n

config CMD_BUTTON
	bool "CMD_BUTTON"
	default n

config CMD_CBI
	bool "CMD_CBI"
	default y

config CMD_CCD_DISABLE
	bool "'ccd disable' subcommand"
	default n
# depends on CASE_CLOSED_DEBUG_V1

config CMD_CHARGEN
	bool "CMD_CHARGEN"
	default n

config CMD_CHARGER
	bool "CMD_CHARGER"
	default y

config CMD_CHARGER_ADC_AMON_BMON
	bool "CMD_CHARGER_ADC_AMON_BMON"
	default n

config CMD_CHARGER_DUMP
	bool "CMD_CHARGER_DUMP"
	default n

config CMD_CHARGER_PROFILE_OVERRIDE
	bool "CMD_CHARGER_PROFILE_OVERRIDE"
	default n

config CMD_CHARGER_PROFILE_OVERRIDE_TEST
	bool "CMD_CHARGER_PROFILE_OVERRIDE_TEST"
	default n

config CMD_CHARGE_SUPPLIER_INFO
	bool "CMD_CHARGE_SUPPLIER_INFO"
	default y

config CMD_CHGRAMP
	bool "CMD_CHGRAMP"
	default n

config CMD_CLOCKGATES
	bool "CMD_CLOCKGATES"
	default n

config CMD_COMXTEST
	bool "CMD_COMXTEST"
	default n

config CMD_CRASH
	bool "CMD_CRASH"
	default y

config CMD_DEVICE_EVENT
	bool "CMD_DEVICE_EVENT"
	default y

config CMD_DLOG
	bool "CMD_DLOG"
	default n

config CMD_ECTEMP
	bool "CMD_ECTEMP"
	default n

config CMD_FASTCHARGE
	bool "CMD_FASTCHARGE"
	default y

config CMD_FLASH
	bool "CMD_FLASH"
	default n

config CMD_FLASHINFO
	bool "CMD_FLASHINFO"
	default y

config CMD_FLASH_LOG
	bool "CMD_FLASH_LOG"
	default n

config CMD_FLASH_TRISTATE
	bool "CMD_FLASH_TRISTATE"
	default n

config CMD_FORCETIME
	bool "CMD_FORCETIME"
	default n

config CMD_GETTIME
	bool "CMD_GETTIME"
	default y

config CMD_GPIO_EXTENDED
	bool "CMD_GPIO_EXTENDED"
	default n

config CMD_GSV
	bool "CMD_GSV"
	default n

config CMD_HASH
	bool "CMD_HASH"
	default y

config CMD_HCDEBUG
	bool "CMD_HCDEBUG"
	default y

config CMD_HOSTCMD
	bool "CMD_HOSTCMD"
	default n

config CMD_I2C_PROTECT
	bool "CMD_I2C_PROTECT"
	default n

config CMD_I2C_SCAN
	bool "CMD_I2C_SCAN"
	default y

config CMD_I2C_STRESS_TEST
	bool "CMD_I2C_STRESS_TEST"
	default n

config CMD_I2C_STRESS_TEST_ACCEL
	bool "CMD_I2C_STRESS_TEST_ACCEL"
	default n

config CMD_I2C_STRESS_TEST_ALS
	bool "CMD_I2C_STRESS_TEST_ALS"
	default n

config CMD_I2C_STRESS_TEST_BATTERY
	bool "CMD_I2C_STRESS_TEST_BATTERY"
	default n

config CMD_I2C_STRESS_TEST_CHARGER
	bool "CMD_I2C_STRESS_TEST_CHARGER"
	default n

config CMD_I2C_STRESS_TEST_TCPC
	bool "CMD_I2C_STRESS_TEST_TCPC"
	default n

config CMD_I2CWEDGE
	bool "CMD_I2CWEDGE"
	default n

config CMD_I2C_XFER
	bool "CMD_I2C_XFER"
	default y

config CMD_IDLE_STATS
	bool "CMD_IDLE_STATS"
	default y

config CMD_ILIM
	bool "CMD_ILIM"
	default n

config CMD_INA
	bool "CMD_INA"
	default y

config CMD_JUMPTAGS
	bool "CMD_JUMPTAGS"
	default n

config CMD_KEYBOARD
	bool "CMD_KEYBOARD"
	default y

config CMD_LEDTEST
	bool "CMD_LEDTEST"
	default n

config CMD_LID_ANGLE
	bool "CMD_LID_ANGLE"
	default n

config CMD_MCDP
	bool "CMD_MCDP"
	default n

config CMD_MD
	bool "CMD_MD"
	default y

config CMD_MEM
	bool "CMD_MEM"
	default y

config CMD_MMAPINFO
	bool "CMD_MMAPINFO"
	default y

config CMD_PD
	bool "CMD_PD"
	default y

config CMD_PD_CONTROL
	bool "CMD_PD_CONTROL"
	default n

config CMD_PD_DEV_DUMP_INFO
	bool "CMD_PD_DEV_DUMP_INFO"
	default n

config CMD_PD_FLASH
	bool "CMD_PD_FLASH"
	default n

config CMD_PECI
	bool "CMD_PECI"
	default y

config CMD_PLL
	bool "CMD_PLL"
	default n

config CMD_PMU
	bool "CMD_PMU"
	default n

config CMD_POWER_AP
	bool "CMD_POWER_AP"
	default y

config CMD_POWERINDEBUG
	bool "CMD_POWERINDEBUG"
	default y

config CMD_POWERLED
	bool "CMD_POWERLED"
	default n

config CMD_PPC_DUMP
	bool "CMD_PPC_DUMP"
	default n

config CMD_PWR_AVG
	bool "CMD_PWR_AVG"
	depends on BATTERY
	default y

config CMD_RAND
	bool "CMD_RAND"
	default n

config CMD_REGULATOR
	bool "CMD_REGULATOR"
	default y

config CMD_RETIMER
	bool "CMD_RETIMER"
	default y

config CMD_RTC
	bool "CMD_RTC"
	default n

config CMD_RTC_ALARM
	bool "CMD_RTC_ALARM"
	default n

config CMD_RW
	bool "CMD_RW"
	default y

config CMD_SCRATCHPAD
	bool "CMD_SCRATCHPAD"
	default n

config CMD_SEVEN_SEG_DISPLAY
	bool "CMD_SEVEN_SEG_DISPLAY"
	default n

config CMD_SHMEM
	bool "CMD_SHMEM"
	default y

config CMD_SLEEP
	bool "CMD_SLEEP"
	default n

config CMD_SLEEPMASK
	bool "CMD_SLEEPMASK"
	default y

config CMD_SLEEPMASK_SET
	bool "CMD_SLEEPMASK_SET"
	default y

config CMD_SPI_FLASH
	bool "CMD_SPI_FLASH"
	default n

config CMD_SPI_NOR
	bool "CMD_SPI_NOR"
	default n

config CMD_SPI_XFER
	bool "CMD_SPI_XFER"
	default n

config CMD_STACKOVERFLOW
	bool "CMD_STACKOVERFLOW"
	default n

config CMD_SYSINFO
	bool "CMD_SYSINFO"
	default y

config CMD_SYSJUMP
	bool "CMD_SYSJUMP"
	default y

config CMD_SYSLOCK
	bool "CMD_SYSLOCK"
	default y

config CMD_TASKREADY
	bool "CMD_TASKREADY"
	default n

config CMD_TASK_RESET
	bool "CMD_TASK_RESET"
	default n

config CMD_TEMP_SENSOR
	bool "CMD_TEMP_SENSOR"
	default y

config CMD_TIMERINFO
	bool "CMD_TIMERINFO"
	default y

config CMD_TYPEC
	bool "CMD_TYPEC"
	default y

config CMD_USART_INFO
	bool "CMD_USART_INFO"
	default n

config CMD_USBMUX
	bool "CMD_USBMUX"
	default y

config CMD_USB_PD_CABLE
	bool "CMD_USB_PD_CABLE"
	default n

config CMD_USB_PD_PE
	bool "CMD_USB_PD_PE"
	default n

config CMD_WAITMS
	bool "CMD_WAITMS"
	default y

