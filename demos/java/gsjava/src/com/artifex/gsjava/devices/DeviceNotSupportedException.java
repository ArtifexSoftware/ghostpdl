package com.artifex.gsjava.devices;

public class DeviceNotSupportedException extends Exception {

	private static final long serialVersionUID = 1L;

	public DeviceNotSupportedException() {
		super();
	}

	public DeviceNotSupportedException(String message) {
		super(message);
	}

	public DeviceNotSupportedException(Throwable cause) {
		super(cause);
	}

	public DeviceNotSupportedException(String message, Throwable cause) {
		super(message, cause);
	}

	protected DeviceNotSupportedException(String message, Throwable cause, boolean enableSuppression,
			boolean writableStackTrace) {
		super(message, cause, enableSuppression, writableStackTrace);
	}

}
