package com.artifex.gsjava.devices;

public class EPSDevice extends PostScriptDevice {

	public EPSDevice() throws IllegalStateException, DeviceNotSupportedException, DeviceInUseException {
		super("eps2write");
	}

}
