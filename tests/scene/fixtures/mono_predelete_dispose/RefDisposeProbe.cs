using Godot;

public partial class RefDisposeProbe : RefCounted
{
    [Signal]
    public delegate void PingEventHandler();

    public static int DisposeCalls { get; private set; }
    private bool _counted;

    protected override void Dispose(bool disposing)
    {
        if (!_counted)
        {
            _counted = true;
            DisposeCalls++;
        }
        base.Dispose(disposing);
    }
}
