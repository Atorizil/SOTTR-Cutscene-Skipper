using System;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using Memory;

namespace SOTTR_Cutscene_Skipper
{
  public partial class Form1 : Form
  {
    #region DllImports
    [DllImport("user32.dll")]
    public static extern bool RegisterHotKey(IntPtr hWnd, int id, int fsModifiers, uint vlc);
    [DllImport("user32.dll")]
    private static extern bool UnregisterHotKey(IntPtr hWnd, int id);
    #endregion

    protected override void WndProc(ref Message msg)
    {
      if (msg.Msg == 0x0312)
      {
        int id = msg.WParam.ToInt32();
        if (id == 1)
        {
          TrySkip();
        }
      }
      base.WndProc(ref msg);
    }

    public static Mem Game = new Mem();
    public Form1()
    {
      InitializeComponent();

      RegisterHotKey(this.Handle, 1, 0x2, 0x20); // ctrl + space
    }

    /// <summary>
    /// When we press space, try to skip the cutscene
    /// </summary>
    public void TrySkip()
    {
      if (IsRunning())
      {
        // Get the pointer to the current timeline instance (cutscene)
        long TimelinePtr = Game.ReadLong("base+0x141B8C0");

        // Check if we are in a cutscene / timeline... Will be used for making some cutscenes unskippable to prevent softlocks / crashes
        if (TimelinePtr != 0)
        {
          // Get the timeline time
          float TimelineTime = Game.ReadFloat("base+0x141B8C0,0x60");

          if (TimelineTime > 6.0f)
          {
            // Write the timeline state(enum: None, Loading, Loaded, Playing, Paused, Skipping)
            Game.WriteMemory("base+0x141B8C0,0x129", "byte", "5");
          }
        }
      } 
    }

    public static bool IsRunning()
    {
      int PID = Game.GetProcIdFromName("SOTTR");
      if (PID > 0)
        return Game.OpenProcess(PID);
      return false;
    }
  }
}
