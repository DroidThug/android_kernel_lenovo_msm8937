GCC_VERSION      := $(shell $(CONFIG_SHELL) $(srctree)/scripts/gcc-version.sh $(CROSS_COMPILE)gcc)

ccflags-y += -Idrivers/misc/lenovo/camera/camera-driver/jpeg_10
ccflags-y += -Idrivers/misc/lenovo/camera/camera-driver/sensor/io
ccflags-y += -Idrivers/misc/lenovo/camera/camera-driver/common

obj-$(CONFIG_MSMB_JPEG) += msm_jpeg_dev.o msm_jpeg_sync.o msm_jpeg_core.o msm_jpeg_hw.o msm_jpeg_platform.o
