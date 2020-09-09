package com.artifex.gsjava.devices;

import static com.artifex.gsjava.GSAPI.*;

import java.util.List;

public class DisplayDevice extends Device {

	public static final String X11 = "x11";
	public static final String X11ALPHA = "x11alpha";
	public static final String X11CMYK = "x11cmyk";
	public static final String X11MONO = "x11mono";
	public static final String X11GRAY2 = "x11gray2";
	public static final String X11GRAY4 = "x11gray4";

	public DisplayDevice() throws IllegalStateException, DeviceNotSupportedException, DeviceInUseException {
		super("display");
	}

	public DisplayDevice(String device) throws IllegalStateException, DeviceNotSupportedException, DeviceInUseException {
		super(device);
	}

	public void setDisplayFormat(int format) {
		setParam("DisplayFormat", format, GS_SPT_INT);
	}

	public void setDisplayResolution(int dpi) {
		setParam("DisplayResolution", dpi, GS_SPT_INT);
	}

	public void setSeparationColorNames(String nameArray) {
		setParam("SeparationColorNames", nameArray, GS_SPT_PARSED);
	}

	public void setSeparationColorNames(String[] names) {
		setSeparationColorNames(toArrayParameter(names));
	}

	public void setSeparationColorNames(List<String> names) {
		setSeparationColorNames(toArrayParameter(names));
	}

	public void setSeparationOrder(String orderArray) {
		setParam("SeparationOrder", orderArray, GS_SPT_PARSED);
	}

	public void setSeparationOrder(String[] order) {
		setSeparationOrder(toArrayParameter(order));
	}

	public void setSeparationOrder(List<String> order) {
		setSeparationOrder(toArrayParameter(order));
	}
}
