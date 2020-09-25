package com.artifex.gsjava.devices;

public class PNMDevice extends FileDevice {

	public static final String PBM = "pbm";
	public static final String PBMRAW = "pbmraw";
	public static final String PGM = "pbm";
	public static final String PGMRAW = "pgmraw";
	public static final String PGMN = "pgnm";
	public static final String PGMNRAW = "pgnmraw";
	public static final String PNM = "pnm";
	public static final String PNMRAW = "pnmraw";
	public static final String PPM = "ppm";
	public static final String PPMRAW = "ppmraw";
	public static final String PKM = "pkm";
	public static final String PKMRAW = "pkmraw";
	public static final String PKSM = "pksm";
	public static final String PKSMRAW = "pksmraw";

	public PNMDevice(String deviceName)
			throws DeviceNotSupportedException, DeviceInUseException, IllegalStateException {
		super(deviceName);
	}

}
