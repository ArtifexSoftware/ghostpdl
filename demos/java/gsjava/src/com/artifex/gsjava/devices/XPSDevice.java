package com.artifex.gsjava.devices;

public class XPSDevice extends FileDevice implements HighLevelDevice {

	public XPSDevice() throws IllegalStateException, DeviceNotSupportedException, DeviceInUseException {
		super("xpswrite");
	}

}
