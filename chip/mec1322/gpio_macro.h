#ifdef DEFINE_GPIO_ENUM
#define GPIO(name, pin, flags, signal) GPIO_##name,
#endif

#ifdef DEFINE_GPIO_INFO_STRUCT
#ifdef CONFIG_COMMON_GPIO_SHORTNAMES
#define GPIO(name, gpionum, flags, signal) \
        {#gpionum, (gpionum/10), (1<<(gpionum%10)), flags, signal},
#else
#define GPIO(name, gpionum, flags, signal) \
        {#name, (gpionum/10), (1<<(gpionum%10)), flags, signal},
#endif
#endif

#ifdef DEFINE_GPIO_ALTERNATE
#define ALTERNATE(pinbase, mask, function, module, flags)	\
	{ pinbase, mask, function, module, flags},
#endif
