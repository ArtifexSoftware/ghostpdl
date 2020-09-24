package com.artifex.gsjava.devices;

import java.awt.Color;

import com.artifex.gsjava.GSAPI;

public class PNGDevice extends FileDevice {

	public static final String PNG256 = "png256";
	public static final String PNG16 = "png16";
	public static final String PNGMONO = "pngmono";
	public static final String PNGMONOD = "pngmonod";
	public static final String PNG16M = "png16m";
	public static final String PNGGRAY = "pnggray";
	public static final String PNGALPHA = "pngalpha";

	public PNGDevice(String format) throws DeviceNotSupportedException, DeviceInUseException, IllegalStateException {
		super(format);
	}

	public void setDownScaleFactor(int value) {
		setParam("DownScaleFactor", value, GSAPI.GS_SPT_INT);
	}

	public void setMinFeatureSize(int state) {
		setParam("MinFeatureSize", state, GSAPI.GS_SPT_INT);
	}

	public void setBackgroundColor(Color color) {
		setBackgroundColor(color.getRGB());
	}

	public void setBackgroundColor(int color) {
		setParam("BackgroundColor", color, GSAPI.GS_SPT_INT);
	}
}
