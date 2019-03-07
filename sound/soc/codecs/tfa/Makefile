ifeq ($(TARGET_BUILD_VARIANT),eng)
EXTRA_CFLAGS += -DSH_DUMP_ENG
EXTRA_CFLAGS += -DDEBUG
endif
obj-$(CONFIG_SND_SOC_NXP_TFA98XX)	+= snd-soc-tfa98xx.o

snd-soc-tfa98xx-objs += tfa98xx.o
snd-soc-tfa98xx-objs += tfa_container.o
snd-soc-tfa98xx-objs += tfa_dsp.o
snd-soc-tfa98xx-objs += tfa9888_init.o
snd-soc-tfa98xx-objs += tfa9891_init.o
snd-soc-tfa98xx-objs += tfa9897_init.o
snd-soc-tfa98xx-objs += tfa9890_init.o
snd-soc-tfa98xx-objs += tfa9887B_init.o
snd-soc-tfa98xx-objs += tfa9887_init.o

ifdef TFA_DEBUG
EXTRA_CFLAGS += -DTFA_DEBUG -DDEBUG
snd-soc-tfa98xx-objs += tfa_debug.o
endif
