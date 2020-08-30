package com.artifex.gsjava.devices;

import static com.artifex.gsjava.GSAPI.*;

import java.awt.Dimension;
import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

import com.artifex.gsjava.GSInstance;
import com.artifex.gsjava.callbacks.IStdErrFunction;
import com.artifex.gsjava.callbacks.IStdInFunction;
import com.artifex.gsjava.callbacks.IStdOutFunction;
import com.artifex.gsjava.util.Reference;

/**
 * The <code>Device</code> class allows easier rendering to a Ghostscript device.
 * The class allows setting different device parameters, running a file, and some
 * safety with regard to Ghostcript instances. There can only be one instance of a
 * <code>Device</code> at a time.
 *
 * @author Ethan Vrhel
 *
 */
public abstract class Device {

	private static Device activeDevice; // The active device, only one device may exist at a time

	private GSInstance instance;
	private final String deviceName;

	/**
	 * Creates a new device with the given device name. When this constructor is called,
	 * an instance of Ghostcript cannot exist or an exception will be thrown. The check of
	 * whether an instance already exists does not include instances directly created
	 * through <code>GSAPI.gsapi_new_instance</code>.
	 *
	 * @param deviceName The name of the Ghostscript device.
	 * @throws DeviceNotSupportedException When <code>deviceName</code> is not a supported
	 * Ghostscript device.
	 * @throws DeviceInUseException When a <code>Device</code> object already exists.
	 * @throws IllegalStateException When an instance of Ghostscript already exists.
	 */
	public Device(String deviceName) throws DeviceNotSupportedException, DeviceInUseException, IllegalStateException {
		if (activeDevice != null)
			throw new DeviceInUseException(activeDevice);
		GSInstance instance = new GSInstance();
		this.deviceName = Objects.requireNonNull(deviceName, "device name must be non-null");
		StdIO io = new StdIO();
		instance.set_stdio(io, io, io);
		instance.init_with_args(new String[] {
			"gs",
			"-h",
		});
		instance.exit();
		instance.delete_instance();
		if (!io.hasDevice)
			throw new DeviceNotSupportedException(deviceName);
	}

	/**
	 * Initializes the device on an instance with the given arguments. The
	 * given instance must not have had <code>init_with_args</code> called.
	 *
	 * @param instance The instance to initialize.
	 * @param more Arguments which should be passed to Ghostscript through
	 * <code>gsapi_init_with_args</code>.
	 */
	public void initWithInstance(GSInstance instance, String... more) {
		if (this.instance != null)
			throw new IllegalStateException("instance has already been initialized");
		this.instance = Objects.requireNonNull(instance, "Instance must be non-null");
		List<String> args = new ArrayList<>(2 + more.length);
		args.add("gs");
		args.add("-sDEVICE=" + deviceName);
		for (String s : more) {
			args.add(s);
		}
		instance.init_with_args(args);
	}

	/**
	 * Runs a file.
	 *
	 * @param file The file to run.
	 * @param userErrors The user errors.
	 * @param pExitCode The exit code.
	 * @return The result of the <code>run_file</code>.
	 */
	public int runFile(File file, int userErrors, Reference<Integer> pExitCode) {
		return runFile(file.getAbsolutePath(), userErrors, pExitCode);
	}

	/**
	 * Runs a file.
	 *
	 * @param file The file to run.
	 * @param userErrors The user errors.
	 * @param pExitCode The exit code.
	 * @return The result of the <code>run_file</code>.
	 */
	public int runFile(String filename, int userErrors, Reference<Integer> pExitCode) {
		return instance.run_file(filename, userErrors, pExitCode);
	}

	/**
	 * Returns the internal device name. This name is the same as the one provided
	 * by the user in the Device constructor.
	 *
	 * @return The internal device name.
	 */
	public String getInternalName() {
		return deviceName;
	}

	/**
	 * Destroys this device.
	 *
	 * @throws IllegalStateException When the instance of Ghostscript fails to
	 * exit.
	 */
	public void destroyDevice() throws IllegalStateException {
		int code = instance.exit();
		if (code != GS_ERROR_OK)
			throw new IllegalStateException("Failed to exit");
		instance.delete_instance();
		activeDevice = null;
	}

	/**
	 * Sets a device parameter.
	 *
	 * @param param The name of the device parameter.
	 * @param value The value to set the device parameter to.
	 * @param paramType The type of the device parameter.
	 * @return The result of setting the parameter through
	 * <code>gsapi_set_param</code>.
	 */
	public int setParam(String param, Object value, int paramType) {
		return instance.set_param(param, value, paramType);
	}

	/**
	 * Sets a string device parameter.
	 *
	 * @param param The name of the device parameter.
	 * @param value The string value to set the device parameter to.
	 * @param paramType The type of the device parameter.
	 * @return The result of setting the parameter through
	 * <code>gsapi_set_param</code>.
	 */
	public int setParam(String param, String value, int paramType) {
		return instance.set_param(param, value, paramType);
	}

	public int setResolution(Dimension resolution) {
		return setResolution("[" + resolution.width + " " + resolution.height + "]");
	}

	public int setResolution(int resolution) {
		return setResolution("[" + resolution + " " + resolution + "]");
	}

	public int setResolution(String resolution) {
		return setParam("HWResolution", resolution, GS_SPT_PARSED);
	}

	public int setFirstPage(int page) {
		return setParam("FirstPage", page, GS_SPT_INT);
	}

	public int setLastPage(int page) {
		return setParam("LastPage", page, GS_SPT_INT);
	}

	public int setTextAlphaBits(int value) {
		return setParam("TextAlphaBits", value, GS_SPT_INT);
	}

	public int setGraphicsAlphaBits(int value) {
		return setParam("GraphicsAlphaBits", value, GS_SPT_INT);
	}

	/**
	 * Returns the stored Ghostscript instance.
	 *
	 * @return The instance.
	 */
	protected final GSInstance getInstance() {
		return instance;
	}

	@Override
	public String toString() {
		return "Device[instance=" + instance + ",deviceName=" + deviceName + "]";
	}

	private class StdIO implements IStdInFunction, IStdOutFunction, IStdErrFunction {

		private boolean expecting = true;
		private boolean hasDevice = false;

		@Override
		public int onStdIn(long callerHandle, byte[] buf, int len) {
			return buf.length;
		}

		@Override
		public int onStdOut(long callerHandle, byte[] str, int len) {
			if (expecting && !hasDevice) {
				String outp = new String(str);
				hasDevice = outp.contains(deviceName);
			}
			return str.length;
		}

		@Override
		public int onStdErr(long callerHandle, byte[] str, int len) {
			if (expecting && !hasDevice) {
				String outp = new String(str);
				hasDevice = outp.contains(deviceName);
			}
			return str.length;
		}

	}
}
