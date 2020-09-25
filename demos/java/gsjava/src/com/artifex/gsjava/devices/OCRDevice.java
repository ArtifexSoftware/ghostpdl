package com.artifex.gsjava.devices;

import com.artifex.gsjava.GSAPI;

public class OCRDevice extends FileDevice {

	public static final int DEFAULT_ENGINE = 0;
	public static final int LSTM_ENGINE = 1;
	public static final int TESSERACT_ENGINE = 2;
	public static final int LSTM_AND_TESSERACT_ENGINE = 3;

	public OCRDevice()
			throws DeviceNotSupportedException, DeviceInUseException, IllegalStateException {
		super("ocr");
	}

	public void setOCRLanguage(String language) {
		setParam("OCRLanguage", language, GSAPI.GS_SPT_PARSED);
	}

	public void setOCREngine(int engine) {
		setParam("OCREngine", engine, GSAPI.GS_SPT_INT);
	}
}
