Imports System

Module Acrobat2Tiff

  Dim PDFApp As Acrobat.AcroApp
  Dim PDDoc As Acrobat.CAcroPDDoc
  Dim AVDoc As Acrobat.CAcroAVDoc
  Dim JSObj As Object, strPDFText As String

  Sub SyntaxFail(ByVal s As String)
    Console.WriteLine(s)
    Console.WriteLine("")
    Console.WriteLine("Syntax: Acrobat2Tiff -i <inputfile> -o <outputfile>")
    Console.WriteLine("  Colorspace will be automatically guessed, but may be specified")
    Console.WriteLine("        using [ -gray | -mono | -rgb | -cmyk ]")
    Console.WriteLine("  Resolution defaults to 72dpi, but may be specified e.g. -r 300")
    Console.WriteLine("")
        Console.WriteLine("Tested with Acrobat 9.0, will hopefully work with 7/8/10/11/DC too.")
        Console.WriteLine("Ensure you have no other Acrobat processes running, or the colorspace")
    Console.WriteLine("and Resolution of output images may be wrong.")
  End Sub

  Sub setRegistryKeys(ByVal regKey As String, ByVal type As String, ByVal res As Integer, ByVal cspace As Integer)
    Dim regKey2 As String
    Dim i As Integer

    i = 0
    While (True)
      regKey2 = regKey & "\c" & CStr(i)
      If My.Computer.Registry.GetValue(regKey2, "tHandlerDescription", Nothing) Is Nothing Then
        Return
      End If
      If My.Computer.Registry.GetValue(regKey2, "tHandlerDescription", Nothing) = type Then
        My.Computer.Registry.SetValue(regKey2 & "\cSettings", "iResolution", res)
        My.Computer.Registry.SetValue(regKey2 & "\cSettings", "iColorSpace", cspace)
        Return
      End If
      i = i + 1
    End While
  End Sub

  Sub Main()
    Dim a_strArgs() As String
    Dim res As Integer
    Dim i As Integer
    Dim cspace As Integer
    Dim infile As String
        Dim outfile As String
        Dim result As Integer

        ' Set defaults
        infile = ""
    outfile = ""
    res = 72
    cspace = 1

    a_strArgs = Split(Command$, " ")
    If (LBound(a_strArgs) <> UBound(a_strArgs)) Then
      For i = LBound(a_strArgs) To UBound(a_strArgs)
        'Console.WriteLine(LCase(a_strArgs(i)))
        Select Case LCase(a_strArgs(i))
          Case "-i", "/i"
            i = i + 1
            infile = a_strArgs(i)
          Case "-o", "/o"
            i = i + 1
            outfile = a_strArgs(i)
          Case "-r", "/r"
            i = i + 1
            res = Val(a_strArgs(i))
          Case "-gray", "/gray", "-grey", "/grey"
            cspace = 3
          Case "-mono", "/mono"
            cspace = 4
          Case "-rgb", "/rgb"
            cspace = 2
          Case "-cmyk", "/cmyk"
            cspace = 5
          Case Else
            SyntaxFail("Unknown argument: '" & a_strArgs(i) & "'")
            Return
        End Select
      Next i
    End If

    If (infile = "") Then
      SyntaxFail("No input file supplied")
      Return
    End If
    If (outfile = "") Then
      SyntaxFail("No output file supplied")
      Return
    End If
    If (res = 0) Then
      SyntaxFail("Invalid resolution!")
      Return
    End If

        Console.WriteLine("Input: '" & infile & "'")
        Console.WriteLine("Output: '" & outfile & "'")
        Console.WriteLine("Resolution: " & CStr(res))
        Console.WriteLine("CSpace: " & CStr(cspace))

        ' Set the registry values
        setRegistryKeys("HKEY_CURRENT_USER\Software\Adobe\Adobe Acrobat\7.0\AVConversionFromPDF\cSettings", "TIFF", res, cspace)
        setRegistryKeys("HKEY_CURRENT_USER\Software\Adobe\Adobe Acrobat\8.0\AVConversionFromPDF\cSettings", "TIFF", res, cspace)
    setRegistryKeys("HKEY_CURRENT_USER\Software\Adobe\Adobe Acrobat\9.0\AVConversionFromPDF\cSettings", "TIFF", res, cspace)
    setRegistryKeys("HKEY_CURRENT_USER\Software\Adobe\Adobe Acrobat\10.0\AVConversionFromPDF\cSettings", "TIFF", res, cspace)
    setRegistryKeys("HKEY_CURRENT_USER\Software\Adobe\Adobe Acrobat\11.0\AVConversionFromPDF\cSettings", "TIFF", res, cspace)
    setRegistryKeys("HKEY_CURRENT_USER\Software\Adobe\Adobe Acrobat\DC\AVConversionFromPDF\cSettings", "TIFF", res, cspace)

    ' Create Acrobat Application object
    PDFApp = CreateObject("AcroExch.App")

    ' Create Acrobat Document object
    PDDoc = CreateObject("AcroExch.PDDoc")

        ' Open PDF file
        result = PDDoc.Open(infile)

        ' Hide Acrobat application so everything is done in silent mode
        'PDFApp.Hide()

        ' Create Javascript bridge object

        JSObj = PDDoc.GetJSObject
        result = PDDoc.GetNumPages

        ' Create Tiff file
        JSObj.SaveAs(outfile, "com.adobe.acrobat.tiff")

    PDDoc.Close()
    PDFApp.CloseAllDocs()

    ' Clean up
    System.Runtime.InteropServices.Marshal.ReleaseComObject(JSObj)
    JSObj = Nothing
    System.Runtime.InteropServices.Marshal.ReleaseComObject(PDFApp)
    PDFApp = Nothing
    System.Runtime.InteropServices.Marshal.ReleaseComObject(PDDoc)
    PDDoc = Nothing
  End Sub

End Module
