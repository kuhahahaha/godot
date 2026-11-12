using Godot;

public partial class Controller : Node
{
	public void DisposeTarget(GodotObject target) => target.Dispose();
	public int NodeDisposeCount() => DisposeProbe.DisposeCalls;
	public int RefDisposeCount() => RefDisposeProbe.DisposeCalls;
	public void SetNested(DisposeProbe owner, DisposeProbe nested) => owner.Nested = nested;
	public void SetExternal(DisposeProbe owner, DisposeProbe external) => owner.External = external;
}
