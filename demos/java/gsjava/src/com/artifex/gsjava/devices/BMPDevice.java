package com.artifex.gsjava.devices;

public class BMPDevice extends FileDevice {

	public static final String BMPMONO = "bmpmono";
	public static final String BMPGRAY = "bmpgray";
	public static final String BMPSEP1 = "bmpsep1";
	public static final String BMPSEP8 = "bmpsep8";
	public static final String BMP16 = "bmp16";
	public static final String BMP256 = "bmp256";
	public static final String BMP16M = "bmp16m";
	public static final String BMP32B = "bmp32b";

	public BMPDevice(String deviceName)
			throws DeviceNotSupportedException, DeviceInUseException, IllegalStateException {
		super(deviceName);
	}
}
