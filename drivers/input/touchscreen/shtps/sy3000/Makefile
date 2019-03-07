ifneq (, $(filter F%, $(SH_BUILD_ID)))
	EXTRA_CFLAGS += -DSHTPS_FACTORY_MODE_ENABLE
endif

ifeq ($(TARGET_BUILD_VARIANT),eng)
	EXTRA_CFLAGS += -DSHTPS_ENGINEER_BUILD_ENABLE
endif

ifeq ($(CONFIG_SHARP_TPS_SY3X00_DEV),y)
	obj-m += shtps.o

	shtps-y	+= 	shtps_spi.o				\
				shtps_i2c.o				\
				shtps_rmi.o				\
				shtps_rmi_sub.o			\
				shtps_rmi_ldm.o			\
				shtps_rmi_devctl.o		\
				shtps_rmi_debug.o

	shtps-y	+= 	shtps_fwctl.o			\
				shtps_filter.o

	shtps-y	+= 	filter/shtps_filter_dynamic_reset_check.o
	shtps-y	+= 	fwctl/shtps_fwctl_s3400.o
	shtps-y	+= 	fwctl/shtps_fwctl_s3700.o
endif
