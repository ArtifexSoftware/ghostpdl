package com.artifex.gsjava.devices;

public class BMPDevice extends FileDevice {

	public static final String BMPMONO = "bmpmono";
	public static final String BMPGRAY = "bmpgray";
	public static final String BMPSEP1 = "bmpsep1";
	public static final String BMPSEP8 = "bmpsep8";
	public static final String BMPSEP16 = "bmpsep16";
	public static final String BMPSEP256 = "bmpsep256";
	public static final String BMPSEP16M = "bmpsep16m";
	public static final String BMPSEP32B = "bmpsep32b";

	public BMPDevice(String deviceName)
			throws DeviceNotSupportedException, DeviceInUseException, IllegalStateException {
		super(deviceName);
	}
}
