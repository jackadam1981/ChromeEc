config FANS__yn
	bool "Select cooling fans"

if FANS__yn

config FANS
	int "Number of cooling fans"
	default 1

config FAN_DSLEEP
	bool "Support fan control while in low-power idle"

config FAN_RPM_CUSTOM
	bool "Use custom fan_percent_to_rpm()"
	help
	  Replace the default fan_percent_to_rpm() function with a board-specific
	  implementation in board.c

config FAN_UPDATE_PERIOD__yn
	bool "Select FAN_UPDATE_PERIOD"
config FAN_UPDATE_PERIOD
	int "FAN_UPDATE_PERIOD"
	depends on FAN_UPDATE_PERIOD__yn
	help
	  We normally check and update the fans once per second (HOOK_SECOND). If this
	  is #defined to a postive integer N, we will only update the fans every N
	  seconds instead.

endif # FANS_yn

config FAN_INIT_SPEED
	int "FAN_INIT_SPEED"
	range 0 100
	default 100
	help
	  Percentage to which all fans are set at initiation.
