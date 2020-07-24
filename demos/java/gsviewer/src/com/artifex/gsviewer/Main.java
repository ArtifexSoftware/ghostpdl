package com.artifex.gsviewer;

import javax.swing.SwingUtilities;
import javax.swing.UIManager;
import javax.swing.UnsupportedLookAndFeelException;

import com.artifex.gsviewer.gui.ViewerWindow;

public class Main {

	public static void main(String[] args) {
		try {
			UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
		} catch (ClassNotFoundException | InstantiationException | IllegalAccessException
				| UnsupportedLookAndFeelException e) {
			System.err.println("Failed to set Look and Feel: " + e);
		}

		ViewerWindow win = new ViewerWindow(new ViewerController());
		SwingUtilities.invokeLater(() -> {
			win.setVisible(true);
		});
	}
}
