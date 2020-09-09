package com.artifex.gsjava.devices;

import static com.artifex.gsjava.GSAPI.*;

public class PDFDevice extends PDFPostscriptDeviceFamily {

	public PDFDevice() throws IllegalStateException, DeviceNotSupportedException, DeviceInUseException {
		super("pdfwrite");
	}

	public void setMaxInlineImageSize(int value) {
		setParam("MaxInlineImageSize", value, GS_SPT_INT);
	}

	public void setDoNumCopies(boolean state) {
		setParam("DoNumCopies", state, GS_SPT_BOOL);
	}

	public void setDetectDuplicateImages(boolean state) {
		setParam("DetectDuplicateImages", state, GS_SPT_BOOL);
	}

	public void setFastWebView(boolean state) {
		setParam("FastWebView", state, GS_SPT_BOOL);
	}

	public void setPreserveAnnots(boolean state) {
		setParam("PreserveAnnots", state, GS_SPT_BOOL);
	}

	public void setPatternImagemask(boolean state) {
		setParam("PatternImagemask", state, GS_SPT_BOOL);
	}

	public void setMaxClipPathSize(int value) {
		setParam("MaxClipPathSize", value, GS_SPT_INT);
	}

	public void setMaxShadingBitmapSize(int value) {
		setParam("MaxShadingBitmapSize", value, GS_SPT_INT);
	}

	public void setHaveTrueTypes(boolean state) {
		setParam("HaveTrueTypes", state, GS_SPT_BOOL);
	}

	public void setHaveTransparency(boolean state) {
		setParam("HaveTransparency", state, GS_SPT_BOOL);
	}

	public void setPDFX(boolean state) {
		setParam("PDFX", state, GS_SPT_BOOL);
	}

	public void setOwnerPassword(String value) {
		setParam("OwnerPassword", value, GS_SPT_STRING);
	}

	public void setUserPassword(String value) {
		setParam("UserPassword", value, GS_SPT_STRING);
	}

	public void setPermissions(int value) {
		setParam("Permissions", value, GS_SPT_INT);
	}

	public void setEncryptionR(int num) {
		setParam("EncryptionR", num, GS_SPT_INT);
	}

	public void setKeyLength(int length) {
		setParam("KeyLength", length, GS_SPT_INT);
	}

	public void setDocumentUUID(String value) {
		setParam("DocumentUUID", value, GS_SPT_STRING);
	}

	public void setInstanceUUID(String value) {
		setParam("InstanceUUID", value, GS_SPT_STRING);
	}

	public void setDocumentTimeSeq(int value) {
		setParam("DocumentTimeSeq", value, GS_SPT_INT);
	}

	public void setDSCEncoding(String encoding) {
		setParam("DSCEncoding", encoding, GS_SPT_STRING);
	}
}
