package com.artifex.gsviewer;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;

import javax.imageio.ImageIO;

public class Main {

	public static void main(String[] args) throws FileNotFoundException {
		Document doc = new Document("010104_momentum_concepts.pdf");
		//doc.loadHighRes(1, 3);
		//doc.unloadHighRes(2);
		doc.loadHighResList(1, 2, 3);

		int pageNum = 1;
		for (Page page : doc) {
			try {
				ImageIO.write(page.getDisplayableImage(), "PNG", new File("page-" + pageNum + ".png"));
			} catch (IOException e) {
				System.err.println("Failed to write page " + pageNum);
			}
			pageNum++;
		}

	}
}
