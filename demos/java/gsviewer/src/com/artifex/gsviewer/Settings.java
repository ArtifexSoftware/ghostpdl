package com.artifex.gsviewer;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.HashMap;
import java.util.Map;

public class Settings {

	public static final String SETTINGS_FILE = "gsviewer.cfg";

	public static final Settings SETTINGS;

	static {
		SETTINGS = new Settings(SETTINGS_FILE);
	}

	private Map<String, Object> settingsMap;
	private String filename;

	private Settings(String filename) {
		this.filename = filename;
		this.settingsMap = new HashMap<>();
		loadDefaultSettings();

		File f = new File(filename);
		if (!f.exists())
			return;

		try {
			InputStream in = new FileInputStream(filename);
			int b;
			ByteArrayOutputStream bout = new ByteArrayOutputStream();

			while ((b = in.read()) != -1) {
				bout.write(b);
			}

			in.close();
			bout.close();

			byte[] array = bout.toByteArray();
			loadFromString(new String(array));
		} catch (IOException e) {
			throw new IllegalStateException("Failed to load settings file: " + SETTINGS_FILE, e);
		}
	}

	private void loadDefaultSettings() {
		putSetting("scrollSens", 50);
		putSetting("preloadRange", 2);
		putSetting("antialiasing", true);
	}

	private void loadFromString(String data) {
		String[] lines = data.split("\n");
		for (int i = 0; i < lines.length; i++) {
			String line = lines[i];
			String[] tokens = line.split(" ");
			if (tokens.length != 3)
				throw new IllegalStateException("Malformed settings file (line " + (i + 1) + ")");
			try {
				switch (tokens[0]) {
				case "int":
					putSetting(tokens[1], Integer.parseInt(tokens[2]));
					break;
				case "double":
					putSetting(tokens[1], Double.parseDouble(tokens[2]));
					break;
				case "bool":
					putSetting(tokens[1], Boolean.parseBoolean(tokens[2]));
					break;
				default:
					throw new IllegalStateException("Invalid type specifier \"" + tokens[0] + "\" (line " + (i + 1) + ")");
				}
			} catch (NumberFormatException e) {
				throw new IllegalStateException("Invalid value \"" + tokens[2] + "\" for type " +
						tokens[0] + " (line " + (i + 1) + ")", e);
			}
		}
	}

	/**
	 * Returns the value of a setting of type name <code>key</code>.
	 *
	 * @param <T> The type of setting stored.
	 * @param key The name of the setting.
	 * @return The respective setting, casted to <code>T</code>.
	 */
	@SuppressWarnings("unchecked")
	public <T> T getSetting(String key) {
		return (T)settingsMap.get(key);
	}

	public void putSetting(String key, Object value) {
		settingsMap.put(key, value);
	}

	public void setSetting(String key, Object value) throws IllegalArgumentException {
		if (settingsMap.get(key) == null) {
			putSetting(key, value);
		} else if (settingsMap.get(key).getClass() == value.getClass()) {
			putSetting(key, value);
		} else {
			throw new IllegalArgumentException("value is not type " +
				settingsMap.get(key).getClass().getName() + " (got " + (value == null ? "null" : value.getClass().getName()) + ")");
		}
	}

	public void save() throws IOException {
		StringBuilder data = new StringBuilder();

		for (String setting : settingsMap.keySet()) {
			Object value = settingsMap.get(setting);
			if (value.getClass() == Integer.class) {
				data.append("int ");
			} else if (value.getClass() == Double.class) {
				data.append("double ");
			} else if (value.getClass() == Boolean.class) {
				data.append("bool ");
			}
			data.append(setting);
			data.append(' ');
			data.append(value);
			data.append('\n');
		}

		OutputStream out = new FileOutputStream(filename);
		out.write(data.toString().getBytes());
		out.close();
	}

	@Override
	public String toString() {
		return settingsMap.toString() + " at " + filename;
	}
}
