using Godot;

public partial class DisposeProbe : Node
{
    [Signal]
    public delegate void PingEventHandler();

    public static int DisposeCalls { get; private set; }
    private bool _counted;
    internal DisposeProbe? Nested;
    internal DisposeProbe? External;

    protected override void Dispose(bool disposing)
    {
        if (!_counted)
        {
            _counted = true;
            DisposeCalls++;
            DisposeProbe? nested = Nested;
            Nested = null;
            nested?.Free();
            DisposeProbe? external = External;
            External = null;
            external?.Dispose();
        }
        base.Dispose(disposing);
    }
}
