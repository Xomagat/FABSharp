public class FabException : Exception
{
    public int Line { get; }
    public FabException(int line, string message) : base(message) { Line = line; }
}