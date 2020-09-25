package com.artifex.gsjava.devices;

import com.artifex.gsjava.GSAPI;

public class TIFFDevice extends FileDevice {

	public static final String TIFFGRAY = "tiffgray";
	public static final String TIFF12NC = "tiff12nc";
	public static final String TIFF24NC = "tiff24nc";
	public static final String TIFF48NC = "tiff48nc";
	public static final String TIFF32NC = "tiff32nc";
	public static final String TIFF64NC = "tiff64nc";
	public static final String TIFFSEP = "tiffsep";

	public static final String TIFFSEP1 = "tiffsep1";
	public static final String TIFFSCALED = "tiffscaled";
	public static final String TIFFSCALED4 = "tiffscaled4";
	public static final String TIFFSCALED8 = "tiffscaled8";
	public static final String TIFFSCALED24 = "tiffscaled24";
	public static final String TIFFSCALED32 = "tiffscaled32";

	public static final String TIFFCRLE = "tiffcrle";
	public static final String TIFFG3 = "tiffg3";
	public static final String TIFFG32D = "tiffg32d";
	public static final String TIFFG4 = "tiffg4";
	public static final String TIFFLZW = "tifflzw";
	public static final String TIFFPACK = "tiffpack";

	public static final String NONE = "none";
	public static final String CRLE = "crle";
	public static final String G3 = "g3";
	public static final String G4 = "g4";
	public static final String LZW = "lzw";
	public static final String PACK = "pack";

	public TIFFDevice(String deviceName)
			throws DeviceNotSupportedException, DeviceInUseException, IllegalStateException {
		super(deviceName);
	}

	public void setMaxStripSize(int size) {
		setParam("MaxStripSize", size, GSAPI.GS_SPT_INT);
	}

	public void setFillOrder(int order) {
		setParam("FillOrder", order, GSAPI.GS_SPT_INT);
	}

	public void setUseBigTIFF(boolean state) {
		setParam("UseBigTIFF", state, GSAPI.GS_SPT_BOOL);
	}

	public void setTIFFDateTime(boolean state) {
		setParam("TIFFDateTime", state, GSAPI.GS_SPT_BOOL);
	}

	public void setCompression(String compression) {
		setParam("Compression", compression, GSAPI.GS_SPT_NAME);
	}

	public void setAdjustWith(int state) {
		setParam("AdjustWidth", state, GSAPI.GS_SPT_INT);
	}

	public void setMinFeatureSize(int state) {
		setParam("MinFeatureSize", state, GSAPI.GS_SPT_INT);
	}

	public void setDownScaleFactor(int factor) {
		setParam("DownScaleFactor", factor, GSAPI.GS_SPT_INT);
	}

	public void setPostRenderProfile(String path) {
		setParam("PostRenderProfile", path, GSAPI.GS_SPT_NAME);
	}

	public void setPrintSpotCMYK(boolean state) {
		setParam("PrintSpotCMYK", state, GSAPI.GS_SPT_BOOL);
	}
}
