package com.artifex.gsjava.devices;

import static com.artifex.gsjava.GSAPI.*;

/**
 * This class is the root device containing methods to set parameters defined
 * in section 6.1 in the
 * <a href="https://ghostscript.com/doc/current/VectorDevices.htm">Vector Devices</a>
 * documentation.
 *
 * @author Ethan Vrhel
 *
 */
public abstract class PDFPostscriptDeviceFamily extends FileDevice implements HighLevelDevice {

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

	public void setColorConversionStrategy(String strat) {
		setParam("ColorConversionSrategy", strat, GS_SPT_STRING);
	}

	public void setColorImageDepth(int value) {
		setParam("ColorImageDepth", value, GS_SPT_INT);
	}

	public void setColorImageDict(String dict) {
		setParam("ColorImageDict", dict, GS_SPT_PARSED);
	}

	public void setColorImageFilter(String filter) {
		setParam("ColorImageFilter", filter, GS_SPT_STRING);
	}

	public void setColorImageDownscaleThreshold(float value) {
		setParam("ColorImageDownscaleThreshold", value, GS_SPT_FLOAT);
	}

	public void setColorImageDownsampleType(String type) {
		setParam("ColorImageDownsampleType", type, GS_SPT_STRING);
	}

	public void setColorImageResolution(int value) {
		setParam("ColorImageResolution", value, GS_SPT_INT);
	}

	public void setCompatabilityLevel(String level) {
		setParam("CompatabilityLevel", level, GS_SPT_PARSED);
	}

	public void setCompressPages(boolean state) {
		setParam("CompressPages", state, GS_SPT_BOOL);
	}

	public void setConvertCMYKImagesToRGB(boolean state) {
		setParam("ConvertCMYKImagesToRGB", state, GS_SPT_BOOL);
	}

	public void setConvertColorImagesToIndexed(boolean state) {
		setParam("ConvertColorImagesToIndexed", state, GS_SPT_BOOL);
	}

	public void setCoreDictVersion(int version) {
		setParam("CoreDictVersion", version, GS_SPT_INT);
	}

	public void setCreateJobTicket(boolean state) {
		setParam("CreateJobTicket", state, GS_SPT_BOOL);
	}

	public void setDefaultRenderingIntent(String intent) {
		setParam("DefaultRenderingIntent", intent, GS_SPT_STRING);
	}

	public void setDetectBlends(boolean state) {
		setParam("DetectBlends", state, GS_SPT_BOOL);
	}

	public void setDoThumbnails(boolean state) {
		setParam("DoThumbnails", state, GS_SPT_BOOL);
	}

	public void setDownsampleColorImages(boolean state) {
		setParam("DownsampleColorImages", state, GS_SPT_BOOL);
	}

	public void setDownsampleGrayImages(boolean state) {
		setParam("DownsampleGrayImages", state, GS_SPT_BOOL);
	}

	public void setDownsampleMonoImages(boolean state) {
		setParam("DownsampleMonoImages", state, GS_SPT_BOOL);
	}

	public void setEmbedAllFonts(boolean state) {
		setParam("EmbedAllFonts", state, GS_SPT_BOOL);
	}

	public void setEmitDSCWarnings(boolean state) {
		setParam("EmitDSCWarnings", state, GS_SPT_BOOL);
	}

	public void setEncodeColorImages(boolean state) {
		setParam("EncodeColorImages", state, GS_SPT_BOOL);
	}

	public void setEncodeGrayImages(boolean state) {
		setParam("EncodeGrayImages", state, GS_SPT_BOOL);
	}

	public void setEncodeMonoImages(boolean state) {
		setParam("EncodeMonoImages", state, GS_SPT_BOOL);
	}

	public void setEndPage(int page) {
		setParam("EndPage", page, GS_SPT_INT);
	}

	public void setGrayACSImageDict(String dict) {
		setParam("GrayACSImageDict", dict, GS_SPT_PARSED);
	}

	public void setGrayImageDepth(int value) {
		setParam("GrayImageDepth", value, GS_SPT_INT);
	}

	public void setGrayImageDict(String dict) {
		setParam("GrayImageDict", dict, GS_SPT_PARSED);
	}

	public void setGrayImageDownsampleThreshold(float thresh) {
		setParam("GrayImageDownsampleThreshold", thresh, GS_SPT_FLOAT);
	}

	public void setGrayImageDownsampleType(String type) {
		setParam("GrayImageDownsampleType", type, GS_SPT_STRING);
	}

	public void setGrayImageFilter(String filter) {
		setParam("GrayImageFilter", filter, GS_SPT_STRING);
	}

	public void setGrayImageResolution(int resolution) {
		setParam("GrayImageResolution", resolution, GS_SPT_INT);
	}

	public void setImageMemory(long memory) {
		setParam("ImageMemory", memory, GS_SPT_SIZE_T);
	}

	public void setLockDistillerParams(boolean state) {
		setParam("LockDistillerParams", state, GS_SPT_BOOL);
	}

	public void setLZWEncodePages(boolean state) {
		setParam("LZWEncodePages", state, GS_SPT_BOOL);
	}

	public void setMaxSubsetPct(int value) {
		setParam("MaxSubsetPct", value, GS_SPT_INT);
	}

	public void setMonoImageDepth(int depth) {
		setParam("MonoImageDepth", depth, GS_SPT_INT);
	}

	public void setMonoImageDict(String dict) {
		setParam("MonoImageDict", dict, GS_SPT_PARSED);
	}

	public void setMonoImageDownsampleThreshold(float thresh) {
		setParam("MonoImageDownsampleThreshold", thresh, GS_SPT_FLOAT);
	}

	public void setMonoImageDownsampleType(String type) {
		setParam("MonoImageDownsampleType", type, GS_SPT_STRING);
	}

	public void setMonoImageFilter(String filter) {
		setParam("MonoImageFilter", filter, GS_SPT_STRING);
	}

	public void setMonoImageResolution(int resolution) {
		setParam("MonoImageResolution", resolution, GS_SPT_INT);
	}

	public void setNeverEmbed(String value) {
		setParam("NeverEmbed", value, GS_SPT_PARSED);
	}

	public void setOffOptimizations(int value) {
		setParam("OffOptimizations", value, GS_SPT_INT);
	}

	public void setOPM(int value) {
		setParam("OPM", value, GS_SPT_INT);
	}

	public void setOptimize(boolean state) {
		setParam("Optimize", state, GS_SPT_BOOL);
	}

	public void setParseDSCComments(boolean state) {
		setParam("ParseDSCComments", state, GS_SPT_BOOL);
	}

	public void setParseDSCCommentsForDocInfo(boolean state) {
		setParam("ParseDSCCommentsForDocInfo", state, GS_SPT_BOOL);
	}

	public void setPreserveCopyPage(boolean state) {
		setParam("PreserveCopyPage", state, GS_SPT_BOOL);
	}

	public void setPreserveEPSInfo(boolean state) {
		setParam("PreserveEPSInfo", state, GS_SPT_BOOL);
	}

	public void setPreserveHalftoneInfo(boolean state) {
		setParam("PreserveHalftoneInfo", state, GS_SPT_BOOL);
	}

	public void setPreserveOPIComments(boolean state) {
		setParam("PreserveOPIComments", state, GS_SPT_BOOL);
	}

	public void setPreserveOverprintSettings(boolean state) {
		setParam("PreserveOverprintSettings", state, GS_SPT_BOOL);
	}

	public void setsRGBProfile(String profile) {
		setParam("sRGBProfile", profile, GS_SPT_PARSED);
	}

	public void setStartPage(int page) {
		setParam("StartPage", page, GS_SPT_INT);
	}

	public void setSubsetFonts(boolean state) {
		setParam("SubsetFonts", state, GS_SPT_BOOL);
	}

	public void setTransferFunctionInfo(String info) {
		setParam("TransferFunctionInfo", info, GS_SPT_STRING);
	}

	public void setUCRandBGInfo(String info) {
		setParam("UCRandBGInfo", info, GS_SPT_STRING);
	}

	public void setUseFlateCompression(boolean state) {
		setParam("UseFlateCompression", state, GS_SPT_BOOL);
	}

	public void setUsePrologue(boolean state) {
		setParam("UsePrologue", state, GS_SPT_BOOL);
	}

	public void setPassThroughJPEGImages(boolean state) {
		setParam("PassThroughJPEGImages", state, GS_SPT_BOOL);
	}

	// Specific to PostScript and PDF input

	public void setPDFSETTINGS(String config) {
		setParam("PDFSETTINGS", config, GS_SPT_STRING);
	}

	// Specific to PCL and PXL input
}