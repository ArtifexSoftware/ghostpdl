package com.artifex.gsjava.devices;

import com.artifex.gsjava.GSAPI;

public class PDFImageDevice extends FileDevice {

	public static final String PDFIMAGE8 = "pdfimage8";
	public static final String PDFIMAGE24 = "pdfimage24";
	public static final String PDFIMAGE32 = "pdfimage32";

	public PDFImageDevice(String deviceName)
			throws DeviceNotSupportedException, DeviceInUseException, IllegalStateException {
		super(deviceName);
	}

	public void setDownScaleFactor(int factor) {
		setParam("DownScaleFactor", factor, GSAPI.GS_SPT_INT);
	}
}
