import java.awt.*;
import java.awt.image.*;
import java.awt.event.*;
import java.io.File;
import javax.swing.*;
import javax.swing.filechooser.*;

public class Nav extends Gview {

    public Nav() {
	initComponents();
	pack();
	pageView = new Gview();
    }

    protected void runMain(String[] args) {
	// NB no error checking.
	pickle.setJob(args[0]);
	origRes = desiredRes = 100/2;
	pickle.setRes(desiredRes, desiredRes);
	pickle.setPageNumber(pageNumber);
	currentPage = pickle.getPrinterOutputPage();
       	setSize(pickle.getImgWidth(), pickle.getImgHeight());
	origW = pickle.getImgWidth();
	origH = pickle.getImgHeight();
	show();

	pageView.runMain(args);
	
    }

    /** main program */
    public static void main( String[] args )
    {
	// if (debug) 
	    System.out.print(usage());
	Nav view = new Nav();
        view.runMain(args);
    }

    /** This method is called from within the constructor to
     * initialize the form.
     */
    private void initComponents() {
	/*
        jMenuBar = new javax.swing.JMenuBar();
        jMenu = new javax.swing.JMenu();
        jMenuItemOpen = new javax.swing.JMenuItem();

        jMenu.setText("File");

	jMenuItemOpen.setText("Open");
	jMenuItemOpen.addActionListener(new java.awt.event.ActionListener() {
                public void actionPerformed(java.awt.event.ActionEvent evt) {
                    jMenuItemOpenActionPerformed(evt);
                }
            }
					);
	jMenu.add(jMenuItemOpen);
	jMenuBar.add(jMenu);
	*/
	addWindowListener(new java.awt.event.WindowAdapter() {
		public void windowClosing(java.awt.event.WindowEvent evt) {
		    exitForm(evt);
		}
	    }
			  );
	/*
	 setJMenuBar(jMenuBar);
	 */
    }
    /** file open */ 
    private void jMenuItemOpenActionPerformed(java.awt.event.ActionEvent evt) {
	runFileOpen();
	int result = chooser.showOpenDialog(null);
	File file = chooser.getSelectedFile();
	if (result == JFileChooser.APPROVE_OPTION) {
	    if (debug) System.out.println("file open " + file.getPath());
	    String[] args = new String[1];
	    args[0] = file.getPath();
	    runMain(args);
	}
    }

    /** file open */ 
    void runFileOpen() {
	int result = chooser.showOpenDialog(null);
	File file = chooser.getSelectedFile();
	if (result == JFileChooser.APPROVE_OPTION) {
	    if (debug) System.out.println("file open " + file.getPath());
	    String[] args = new String[1];
	    args[0] = file.getPath();
	    runMain(args);
	}
    }

    /** Exit the Application */
    private void exitForm(java.awt.event.WindowEvent evt) {
        System.exit (0);
    }

    /**
     * usage:
    public static String usage() {
	String str =
	    "Usage::java Gview file.pcl\n"
	    + "q ->quit\n"
	    + "drag mouse1 -> translate\n"
	    + "crtl mouse1 -> reset zoom \n"
	    + "mouse2 -> zoom in\n"
	    ;
	return str;
    }
     */

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
	
	// System.out.println( "tx " +  psfx +
	//	    	     " ty " + psfy );

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
    }

    protected void zoomOut( int x, int y ) {
        double psfx = x / origW * pageView.origW / pageView.origRes * pageView.desiredRes;
        double psfy = y / origH * pageView.origH / pageView.origRes * pageView.desiredRes;
	pageView.origX = pageView.origY = 0;
	pageView.zoomOut(0,0);
    }

    /**
     * @link aggregationByValue
     */
    private Gview pageView;

    private JFileChooser chooser = new JFileChooser();
    private javax.swing.JMenuBar jMenuBar;
    private javax.swing.JMenu jMenu;
    private javax.swing.JMenuItem jMenuItemOpen;


}
