using System.Globalization;
using System.Windows.Data;

namespace VI.Module.Intervention;

// 原绑定已包含单位；仅拆分显示字号，保留其数值、单位和无状态时的破折号。
public sealed class ReadoutPartConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        string[] parts = ((string)value).Split(' ', 2);
        return (string)parameter == "Value" ? parts[0] : string.Join(" ", parts.Skip(1));
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture) =>
        throw new NotSupportedException();
}
