package com.artifex.gsjava.devices;

import com.artifex.gsjava.GSAPI;

public class TextDevice extends FileDevice implements HighLevelDevice {

	public TextDevice() throws IllegalStateException, DeviceNotSupportedException, DeviceInUseException {
		super("txtwrite");
	}

	public void setTextFormat(int format) {
		setParam("TextFormat", format, GSAPI.GS_SPT_INT);
	}
}
