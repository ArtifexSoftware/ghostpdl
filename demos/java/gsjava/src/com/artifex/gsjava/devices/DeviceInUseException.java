package com.artifex.gsjava.devices;

public class DeviceInUseException extends Exception {

	private static final long serialVersionUID = 1L;

	private Device device;

	public DeviceInUseException(Device device) {
		super(device == null ? "null" : device.toString());
		this.device = device;
	}

	public Device getDevice() {
		return device;
	}
}
