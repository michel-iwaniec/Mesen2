using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Media.TextFormatting;
using Avalonia.Platform;
using Avalonia.Rendering.SceneGraph;
using Avalonia.Skia;
using Mesen.Utilities;
using SkiaSharp;
using System;
using System.Collections.Generic;

namespace Mesen.Controls
{
	public class SimpleImageViewer : Control
	{
		public static readonly StyledProperty<DynamicBitmap> SourceProperty = AvaloniaProperty.Register<SimpleImageViewer, DynamicBitmap>(nameof(Source));

		public DynamicBitmap Source
		{
			get { return GetValue(SourceProperty); }
			set { SetValue(SourceProperty, value); }
		}

		static SimpleImageViewer()
		{
			AffectsRender<SimpleImageViewer>(SourceProperty);
		}

		public SimpleImageViewer()
		{
		}

		public override void Render(DrawingContext context)
		{
			if(Source == null) {
				return;
			}

			context.Custom(new DrawOperation(this));
		}

		class DrawOperation : ICustomDrawOperation
		{
			public Rect Bounds { get; private set; }

			private DynamicBitmap _source;
			private SKBitmap _bitmap;

			public DrawOperation(SimpleImageViewer viewer)
			{
				Bounds = viewer.Bounds;
				_source = (DynamicBitmap)viewer.Source;
				using(var lockedBuffer = ((WriteableBitmap)_source).Lock()) {
					var info = new SKImageInfo(
						lockedBuffer.Size.Width,
						lockedBuffer.Size.Height,
						lockedBuffer.Format.ToSkColorType(),
						SKAlphaType.Premul
					);
					_bitmap = new SKBitmap();
					_bitmap.InstallPixels(info, lockedBuffer.Address);
				}
			}

			public void Dispose()
			{
				_bitmap.Dispose();
			}

			public bool Equals(ICustomDrawOperation? other) => false;
			public bool HitTest(Point p) => true;

			public void Render(ImmediateDrawingContext context)
			{
				var leaseFeature = context.PlatformImpl.GetFeature<ISkiaSharpApiLeaseFeature>();
				if(leaseFeature != null) {
					using var lease = leaseFeature.Lease();
					var canvas = lease.SkCanvas;

					int width = (int)(_source.Size.Width);
					int height = (int)(_source.Size.Height);

					using(_source.Lock(true)) {
						canvas.DrawBitmap(_bitmap,
							new SKRect(0, 0, (int)_source.Size.Width, (int)_source.Size.Height),
							new SKRect(0, 0, (float)Bounds.Width, (float)Bounds.Height)
						);
					}
				}
			}
		}
	}
}
