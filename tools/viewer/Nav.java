import java.awt.*;
import java.awt.image.*;
import java.awt.event.*;
import java.io.File;
import javax.swing.*;
import javax.swing.filechooser.*;

/** 
 * Two window nav + zoomed Viewer for PCL and PXL files.
 * 
 * Usage:
 * java Nav ../frs96.pxl
 * 
 * Adds a smaller navigation window coupled to a Gview window.
 * This allows a small portion of a page to be viewed at high resolution
 * in the Gview window with navigation occuring via the Nav window.
 * 
 * Mostly inherits behavior and reflects action on this window and the 
 * Gview zoomed window.
 *
 * @version $Revision$
 * @author Stefan Kemper
 */
public class Nav extends Gview {

    public Nav() {
	pageView = new Gview();
    }

    protected void runMain(String[] args) {
	// NB no error checking.
	pickle.setJob(args[0]);
	origRes = desiredRes = startingRes/2;
	pickle.setRes(desiredRes, desiredRes);
	pageNumber = 1;
	pickle.setPageNumber(pageNumber);
	currentPage = pickle.getPrinterOutputPage();
	setSize(pickle.getImgWidth(), pickle.getImgHeight());
	origW = pickle.getImgWidth();
	origH = pickle.getImgHeight();
	show();

	pageView.runMain(args);
	repaint();
    }

    /** main program */
    public static void main( String[] args )
    {
	// if (debug)
	    System.out.print(usage());
	Nav view = new Nav();
        view.runMain(args);
    }

    public void nextPage() {
	super.nextPage();
	
	pageView.setPage(pageNumber);
    }

    public void prevPage() {
        super.prevPage();
	
	pageView.setPage(pageNumber);
    }

    public void imageIsReady( BufferedImage newImage ) {
	super.imageIsReady(newImage);

	if (pickle.busy() == false) {
	    pageView.pickle.startProduction(pageNumber);
	}
    }

    protected void translate(int x, int y) {
	origX -= x;
	origY -= y;
	
        double x1 = origX;
        double y1 = origY;

        double sfx = desiredRes / origRes;
        double sfy = desiredRes / origRes;

        double psfx = origX / origW * pageView.origW / pageView.origRes * pageView.desiredRes;
        double psfy = origY / origH * pageView.origH / pageView.origRes * pageView.desiredRes;
	
	pageView.translateTo( psfx, psfy );
	repaint();
    }

    public void paint( Graphics g )
    {
	g.drawImage(currentPage, 0, 0, this);
	int h = (int)(origH * pageView.origRes / pageView.desiredRes);
        int w = (int)(origW * pageView.origRes / pageView.desiredRes);
        g.setColor(Color.red);  	
	g.drawRect((int)origX, (int)origY, w, h);
    }

    protected void zoomIn( int x, int y ) {

        double psfx = x / origW * pageView.origW / pageView.origRes * pageView.desiredRes;
        double psfy = y / origH * pageView.origH / pageView.origRes * pageView.desiredRes;
	pageView.origX = pageView.origY = 0;
	pageView.zoomIn((int)psfx,(int)psfy);
	repaint();
    }

    protected void zoomOut( int x, int y ) {
        double psfx = x / origW * pageView.origW / pageView.origRes * pageView.desiredRes;
        double psfy = y / origH * pageView.origH / pageView.origRes * pageView.desiredRes;
	pageView.origX = pageView.origY = 0;
	pageView.zoomOut(0,0);
	repaint();
    }

    /**
     * High resolution window.
     * @link aggregationByValue
     */
    private Gview pageView;
}
