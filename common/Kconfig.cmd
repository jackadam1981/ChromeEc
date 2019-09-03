
config CMD_ACCEL_FIFO
	bool "CMD_ACCEL_FIFO"

config CMD_ACCEL_INFO
	bool "CMD_ACCEL_INFO"

config CMD_ACCELS
	bool "CMD_ACCELS"

config CMD_ACCELSPOOF
	bool "CMD_ACCELSPOOF"
	default y

config CMD_ADC
	bool "CMD_ADC"
	depends on ADC
	default y

config CMD_ALS
	bool "CMD_ALS"

config CMD_AP_RESET_LOG
	bool "CMD_AP_RESET_LOG"

config CMD_APTHROTTLE
	bool "CMD_APTHROTTLE"
	default y

config CMD_BATDEBUG
	bool "CMD_BATDEBUG"

config CMD_BATTFAKE
	bool "CMD_BATTFAKE"
	default y

config CMD_BATT_MFG_ACCESS
	bool "CMD_BATT_MFG_ACCESS"

config CMD_BUTTON
	bool "CMD_BUTTON"

config CMD_CBI
	bool "CMD_CBI"
	default y

config CMD_CCD_DISABLE
	bool "'ccd disable' subcommand"
# depends on CASE_CLOSED_DEBUG_V1

config CMD_CHARGEN
	bool "CMD_CHARGEN"

config CMD_CHARGER
	bool "CMD_CHARGER"
	default y

config CMD_CHARGER_ADC_AMON_BMON
	bool "CMD_CHARGER_ADC_AMON_BMON"

config CMD_CHARGER_DUMP
	bool "CMD_CHARGER_DUMP"

config CMD_CHARGER_PROFILE_OVERRIDE
	bool "CMD_CHARGER_PROFILE_OVERRIDE"

config CMD_CHARGER_PROFILE_OVERRIDE_TEST
	bool "CMD_CHARGER_PROFILE_OVERRIDE_TEST"

config CMD_CHARGE_SUPPLIER_INFO
	bool "CMD_CHARGE_SUPPLIER_INFO"
	default y

config CMD_CHGRAMP
	bool "CMD_CHGRAMP"

config CMD_CLOCKGATES
	bool "CMD_CLOCKGATES"

config CMD_COMXTEST
	bool "CMD_COMXTEST"

config CMD_CRASH
	bool "CMD_CRASH"
	default y

config CMD_DEVICE_EVENT
	bool "CMD_DEVICE_EVENT"
	default y

config CMD_DLOG
	bool "CMD_DLOG"

config CMD_ECTEMP
	bool "CMD_ECTEMP"

config CMD_FASTCHARGE
	bool "CMD_FASTCHARGE"
	default y

config CMD_FLASH
	bool "CMD_FLASH"

config CMD_FLASHINFO
	bool "CMD_FLASHINFO"
	default y

config CMD_FLASH_LOG
	bool "CMD_FLASH_LOG"

config CMD_FLASH_TRISTATE
	bool "CMD_FLASH_TRISTATE"

config CMD_FORCETIME
	bool "CMD_FORCETIME"

config CMD_GETTIME
	bool "CMD_GETTIME"
	default y

config CMD_GPIO_EXTENDED
	bool "CMD_GPIO_EXTENDED"

config CMD_GSV
	bool "CMD_GSV"

config CMD_HASH
	bool "CMD_HASH"
	default y

config CMD_HCDEBUG
	bool "CMD_HCDEBUG"
	default y

config CMD_HOSTCMD
	bool "CMD_HOSTCMD"

config CMD_I2C_PROTECT
	bool "CMD_I2C_PROTECT"

config CMD_I2C_SCAN
	bool "CMD_I2C_SCAN"
	default y

config CMD_I2C_STRESS_TEST
	bool "CMD_I2C_STRESS_TEST"

config CMD_I2C_STRESS_TEST_ACCEL
	bool "CMD_I2C_STRESS_TEST_ACCEL"

config CMD_I2C_STRESS_TEST_ALS
	bool "CMD_I2C_STRESS_TEST_ALS"

config CMD_I2C_STRESS_TEST_BATTERY
	bool "CMD_I2C_STRESS_TEST_BATTERY"

config CMD_I2C_STRESS_TEST_CHARGER
	bool "CMD_I2C_STRESS_TEST_CHARGER"

config CMD_I2C_STRESS_TEST_TCPC
	bool "CMD_I2C_STRESS_TEST_TCPC"

config CMD_I2CWEDGE
	bool "CMD_I2CWEDGE"

config CMD_I2C_XFER
	bool "CMD_I2C_XFER"
	default y

config CMD_IDLE_STATS
	bool "CMD_IDLE_STATS"
	default y

config CMD_ILIM
	bool "CMD_ILIM"

config CMD_INA
	bool "CMD_INA"
	default y

config CMD_JUMPTAGS
	bool "CMD_JUMPTAGS"

config CMD_KEYBOARD
	bool "CMD_KEYBOARD"
	default y

config CMD_LEDTEST
	bool "CMD_LEDTEST"

config CMD_LID_ANGLE
	bool "CMD_LID_ANGLE"

config CMD_MCDP
	bool "CMD_MCDP"

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

config CMD_PD_DEV_DUMP_INFO
	bool "CMD_PD_DEV_DUMP_INFO"

config CMD_PD_FLASH
	bool "CMD_PD_FLASH"

config CMD_PECI
	bool "CMD_PECI"
	default y

config CMD_PLL
	bool "CMD_PLL"

config CMD_PMU
	bool "CMD_PMU"

config CMD_POWER_AP
	bool "CMD_POWER_AP"
	default y

config CMD_POWERINDEBUG
	bool "CMD_POWERINDEBUG"
	default y

config CMD_POWERLED
	bool "CMD_POWERLED"

config CMD_PPC_DUMP
	bool "CMD_PPC_DUMP"

config CMD_PWR_AVG
	bool "CMD_PWR_AVG"
	depends on BATTERY
	default y

config CMD_RAND
	bool "CMD_RAND"

config CMD_REGULATOR
	bool "CMD_REGULATOR"
	default y

config CMD_RETIMER
	bool "CMD_RETIMER"
	default y

config CMD_RTC
	bool "CMD_RTC"

config CMD_RTC_ALARM
	bool "CMD_RTC_ALARM"

config CMD_RW
	bool "CMD_RW"
	default y

config CMD_SCRATCHPAD
	bool "CMD_SCRATCHPAD"

config CMD_SEVEN_SEG_DISPLAY
	bool "CMD_SEVEN_SEG_DISPLAY"

config CMD_SHMEM
	bool "CMD_SHMEM"
	default y

config CMD_SLEEP
	bool "CMD_SLEEP"

config CMD_SLEEPMASK
	bool "CMD_SLEEPMASK"
	default y

config CMD_SLEEPMASK_SET
	bool "CMD_SLEEPMASK_SET"
	default y

config CMD_SPI_FLASH
	bool "CMD_SPI_FLASH"

config CMD_SPI_NOR
	bool "CMD_SPI_NOR"

config CMD_SPI_XFER
	bool "CMD_SPI_XFER"

config CMD_STACKOVERFLOW
	bool "CMD_STACKOVERFLOW"

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

config CMD_TASK_RESET
	bool "CMD_TASK_RESET"

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

config CMD_USBMUX
	bool "CMD_USBMUX"
	default y

config CMD_USB_PD_CABLE
	bool "CMD_USB_PD_CABLE"

config CMD_USB_PD_PE
	bool "CMD_USB_PD_PE"

config CMD_WAITMS
	bool "CMD_WAITMS"
	default y
