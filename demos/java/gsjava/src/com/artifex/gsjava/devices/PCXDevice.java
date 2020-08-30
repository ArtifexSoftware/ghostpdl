package com.artifex.gsjava.devices;

public class PCXDevice extends FileDevice {

	public static final String PCXMONO = "pcxmono";
	public static final String PCXGRAY = "pcxgray";
	public static final String PCX16 = "pcx16";
	public static final String PCX256 = "pcx256";
	public static final String PCX24B = "pcx24b";
	public static final String PCXCMYK = "pcxcmyk";

	public PCXDevice(String deviceName)
			throws DeviceNotSupportedException, DeviceInUseException, IllegalStateException {
		super(deviceName);
	}

}
