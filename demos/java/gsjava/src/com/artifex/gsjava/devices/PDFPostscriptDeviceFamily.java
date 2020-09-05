package com.artifex.gsjava.devices;

import static com.artifex.gsjava.GSAPI.*;

public abstract class PDFPostscriptDeviceFamily extends FileDevice {

	public PDFPostscriptDeviceFamily(String device)
			throws IllegalStateException, DeviceNotSupportedException, DeviceInUseException {
		super(device);
	}

	public void setCompressFonts(boolean state) {
		setParam("CompressFonts", state, GS_SPT_BOOL);
	}

	public void setCompressStreams(boolean state) {
		setParam("CompressStreas", state, GS_SPT_BOOL);
	}

	public void setAlwaysEmbed(String value) {
		setParam("AlwaysEmbed", value, GS_SPT_PARSED);
	}

	public void setAntiAliasColorImages(boolean state) {
		setParam("AntiAliasColorImages", state, GS_SPT_BOOL);
	}

	public void setAntiAliasGrayImages(boolean state) {
		setParam("AntiAliasGrayImages", state, GS_SPT_BOOL);
	}

	public void setAntiAliasMonoImages(boolean state) {
		setParam("AntiAliasMonoImages", state, GS_SPT_BOOL);
	}

	public void setASCII85EncodePages(boolean state) {
		setParam("ASCII85EncodePages", state, GS_SPT_BOOL);
	}

	public void setAutoFilterColorImages(boolean state) {
		setParam("AutoFilterColorImages", state, GS_SPT_BOOL);
	}

	public void setAutoFilterGrayImages(boolean state) {
		setParam("AutoFilterGrayImages", state, GS_SPT_BOOL);
	}

	public void setAutoPositionEPSFiles(boolean state) {
		setParam("AutoPositionEPSFiles", state, GS_SPT_BOOL);
	}

	public void setAutoRotatePages(String mode) {
		setParam("AutoRotatePages", mode, GS_SPT_STRING);
	}

	public void setBinding(String mode) {
		setParam("Binding", mode, GS_SPT_STRING);
	}

	public void setCalCMYKProfile(String profile) {
		setParam("CalCMYKProfile", profile, GS_SPT_PARSED);
	}

	public void setCalGrayProfile(String profile) {
		setParam("CalGrayProfile", profile, GS_SPT_PARSED);
	}

	public void setCalRGBProfile(String profile) {
		setParam("CalRGBProfile", profile, GS_SPT_PARSED);
	}

	public void setCannotEmbedFontPolicy(String mode) {
		setParam("CannotEmbedFontPolicy", mode, GS_SPT_STRING);
	}

	public void setColorACSImageDict(String dict) {
		setParam("ColorACSImageDict", dict, GS_SPT_PARSED);
	}
}
