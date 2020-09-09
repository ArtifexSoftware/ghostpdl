package com.artifex.gsjava.devices;

import static com.artifex.gsjava.GSAPI.*;

public class PostScriptDevice extends PDFPostscriptDeviceFamily {

	public PostScriptDevice() throws IllegalStateException, DeviceNotSupportedException, DeviceInUseException {
		super("ps2write");
	}

	/**
	 * Intended to be an internal way to override the normal
	 * ps2write device.
	 *
	 * @param override The device override name.
	 * @throws IllegalStateException {@link Device#Device(String)}
	 * @throws DeviceNotSupportedException {@link Device#Device(String)}
	 * @throws DeviceInUseException {@link Device#Device(String)}
	 */
	protected PostScriptDevice(String override) throws IllegalStateException, DeviceNotSupportedException, DeviceInUseException {
		super(override);
	}

//	public void setPSDocOptions(String options) {
//		setParam("PSDocOptions", options, GS_SPT_STRING);
//	}
//
//	public void setPSPageOptions(String[] options) {
//
//	}
//
//	public void setPSPageOptions(List<String> options) {
//		setPSPageOptions(options.toArray(new String[options.size()]));
//	}

	public void setProduceDSC(boolean state) {
		setParam("ProduceDSC", state, GS_SPT_BOOL);
	}

	public void setCompressEntireFile(boolean state) {
		setParam("CompressEntireFile", state, GS_SPT_BOOL);
	}

	public void setRotatePages(boolean state) {
		setParam("RotatePages", state, GS_SPT_BOOL);
	}

	public void setCenterPages(boolean state) {
		setParam("CenterPages", state, GS_SPT_BOOL);
	}

	public void setSetPageSize(boolean state) {
		setParam("SetPageSize", state, GS_SPT_BOOL);
	}

	public void setDoNumCopies(boolean state) {
		setParam("DoNumCopies", state, GS_SPT_BOOL);
	}
}
