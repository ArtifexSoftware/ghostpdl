package com.artifex.gsjava.devices;

import java.io.File;

import com.artifex.gsjava.GSAPI;
import com.artifex.gsjava.GSInstance;

public abstract class FileDevice extends Device {

	public FileDevice(String deviceName)
			throws DeviceNotSupportedException, DeviceInUseException, IllegalStateException {
		super(deviceName);
	}

	public void initFileWithInstance(GSInstance instance, File file, String... more) {
		initFileWithInstance(instance, file, more);
	}

	public void setOutputFile(File file) {
		setOutputFile(file.getAbsolutePath());
	}

	public void setOutputFile(String file) {
		setParam("OutputFile", file, GSAPI.GS_SPT_STRING);
	}

	public void initFileWithInstance(GSInstance instance, String file, String... more) {
		String[] args = new String[2 + more.length];
		args[0] = "-o";
		args[1] = file;
		for (int i = 0; i < more.length; i++) {
			args[i + 2] = more[i];
		}
		initWithInstance(instance, args);
	}
}
