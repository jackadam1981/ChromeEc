#ifndef EMUL_POWER_SIGNALS_H_
#define EMUL_POWER_SIGNALS_H_

#define AP_PWR_TEST_PLATFORM_ENUM_WITH_COMA(inst)                             \
	DT_STRING_TOKEN(inst, name_id),

enum test_platforms_id {
DT_FOREACH_STATUS_OKAY(ap_pwr_test_platform,
		       AP_PWR_TEST_PLATFORM_ENUM_WITH_COMA)
	TEST_ID_COUNT,
};

int power_signal_emul_load(enum test_platforms_id test_id);

int power_signal_emul_unload(void);

#endif // EMUL_POWER_SIGNALS_H_

