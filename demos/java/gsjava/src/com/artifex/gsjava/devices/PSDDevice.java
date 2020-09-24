package com.artifex.gsjava.devices;

public class PSDDevice extends FileDevice {

	public static final String PSDCMYK = "psdcmyk";
	public static final String PSDRGB = "psdrgb";
	public static final String PSDCMYK16 = "psdcmyk16";
	public static final String PSDRGB16 = "psdrgb16";
	public static final String PSDCMYKOG = "psdcmykog";

	public PSDDevice(String deviceName)
			throws DeviceNotSupportedException, DeviceInUseException, IllegalStateException {
		super(deviceName);
	}

	// Missing device parameters
}
