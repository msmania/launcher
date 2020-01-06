!IF "$(PLATFORM)"=="X64" || "$(PLATFORM)"=="x64"
ARCH=amd64
!ELSE
ARCH=x86
!ENDIF

OUTDIR=bin\$(ARCH)
OBJDIR=obj\$(ARCH)
SRCDIR=src

CC=cl
RD=rd/s/q
RM=del/q
LINKER=link
TARGET=t.exe

OBJS=\
	$(OBJDIR)\blob.obj\
	$(OBJDIR)\main.obj\

LIBS=\
	ole32.lib\
	shell32.lib\
	shlwapi.lib\
	user32.lib\
	comsuppw.lib\

# warning C4100: unreferenced formal parameter
CFLAGS=\
	/nologo\
	/c\
	/DUNICODE\
	/D_CRT_SECURE_NO_WARNINGS\
#	/DDOWNLEVEL\
	/Od\
	/W4\
	/Zi\
	/EHsc\
	/Fo"$(OBJDIR)\\"\
	/Fd"$(OBJDIR)\\"\
	/wd4100\
	/guard:cf\

LFLAGS=\
	/NOLOGO\
	/DEBUG\
	/SUBSYSTEM:CONSOLE\
#	/GUARD:CF\
#	/DYNAMICBASE:NO\
#	/BASE:0x200000000\

all: $(OUTDIR)\$(TARGET)

$(OUTDIR)\$(TARGET): $(OBJS)
	@if not exist $(OUTDIR) mkdir $(OUTDIR)
	$(LINKER) $(LFLAGS) $(LIBS) /PDB:"$(@R).pdb" /OUT:$@ $**

{$(SRCDIR)}.cpp{$(OBJDIR)}.obj:
	@if not exist $(OBJDIR) mkdir $(OBJDIR)
	$(CC) $(CFLAGS) $<

clean:
	@if exist $(OBJDIR) $(RD) $(OBJDIR)
	@if exist $(OUTDIR)\$(TARGET) $(RM) $(OUTDIR)\$(TARGET)
	@if exist $(OUTDIR)\$(TARGET:exe=ilk) $(RM) $(OUTDIR)\$(TARGET:exe=ilk)
	@if exist $(OUTDIR)\$(TARGET:exe=pdb) $(RM) $(OUTDIR)\$(TARGET:exe=pdb)
