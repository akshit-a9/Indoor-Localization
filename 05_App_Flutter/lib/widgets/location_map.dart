import 'package:flutter/material.dart';
import '../services/coordinates_service.dart';

class LocationMap extends StatefulWidget {
  final LocationCoord? coord;

  const LocationMap({super.key, this.coord});

  @override
  State<LocationMap> createState() => _LocationMapState();
}

class _LocationMapState extends State<LocationMap> {
  // The base dimensions of your PNG maps
  static const double _mapW = 1536.0;
  static const double _mapH = 1024.0;
  
  static const List<String> _mapAssets = [
    'assets/Maps/map_0.png',
    'assets/Maps/map_1.png',
  ];

  final TransformationController _controller = TransformationController();
  bool _isUserInteracting = false;
  LocationCoord? _lastAutoZoomedCoord;

  @override
  void didUpdateWidget(LocationMap oldWidget) {
    super.didUpdateWidget(oldWidget);
    
    // Auto-zoom to a new coordinate if the user isn't manual-panning
    if (widget.coord != null && widget.coord != _lastAutoZoomedCoord) {
      if (!_isUserInteracting || _lastAutoZoomedCoord == null) {
        _zoomToCoord(widget.coord!);
      }
      _lastAutoZoomedCoord = widget.coord;
    } else if (widget.coord == null) {
      _lastAutoZoomedCoord = null;
    }
  }

  void _zoomToCoord(LocationCoord coord) {
    // Zoom in (2.5x) and center on the marker
    const double scale = 2.5;
    
    Future.microtask(() {
      if (!mounted) return;
      final RenderBox? renderBox = context.findRenderObject() as RenderBox?;
      if (renderBox == null) return;
      
      final size = renderBox.size;
      final sx = size.width / _mapW;
      final sy = size.height / _mapH;

      final x = coord.x * sx;
      final y = coord.y * sy;

      final matrix = Matrix4.identity()
        ..translate(size.width / 2 - x * scale, size.height / 2 - y * scale)
        ..scale(scale);
      
      setState(() {
        _controller.value = matrix;
        _isUserInteracting = false; 
      });
    });
  }

  void _resetToFullView() {
    setState(() {
      _controller.value = Matrix4.identity();
      _isUserInteracting = true; // Stay in full view until manually centered or new location arrives
    });
  }

  @override
  Widget build(BuildContext context) {
    final mapIdx = (widget.coord?.map ?? 0).clamp(0, _mapAssets.length - 1);
    final asset = _mapAssets[mapIdx];
    final floorName = mapIdx == 0 ? 'Ground Floor' : 'First Floor';

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(left: 4, bottom: 4),
          child: Row(
            children: [
              const Icon(Icons.layers_outlined, size: 16),
              const SizedBox(width: 6),
              Text(
                floorName,
                style: Theme.of(context).textTheme.labelLarge?.copyWith(
                      fontWeight: FontWeight.bold,
                      color: Theme.of(context).colorScheme.primary,
                    ),
              ),
              const Spacer(),
              if (widget.coord != null)
                TextButton.icon(
                  onPressed: () => _zoomToCoord(widget.coord!),
                  icon: const Icon(Icons.center_focus_strong, size: 16),
                  label: const Text('Center', style: TextStyle(fontSize: 12)),
                  style: TextButton.styleFrom(visualDensity: VisualDensity.compact),
                ),
              TextButton.icon(
                onPressed: _resetToFullView,
                icon: const Icon(Icons.map, size: 16),
                label: const Text('Full Map', style: TextStyle(fontSize: 12)),
                style: TextButton.styleFrom(visualDensity: VisualDensity.compact),
              ),
            ],
          ),
        ),
        AspectRatio(
          aspectRatio: _mapW / _mapH,
          child: Container(
            decoration: BoxDecoration(
              borderRadius: BorderRadius.circular(16),
              border: Border.all(color: Theme.of(context).colorScheme.outlineVariant),
              color: Theme.of(context).colorScheme.surfaceContainerLow,
            ),
            clipBehavior: Clip.antiAlias,
            child: InteractiveViewer(
              transformationController: _controller,
              maxScale: 5.0,
              minScale: 1.0, // Prevent zooming out past the map bounds
              panEnabled: true,
              scaleEnabled: true,
              constrained: true, // Keep the content constrained
              boundaryMargin: const EdgeInsets.all(double.infinity),
              onInteractionStart: (_) {
                setState(() => _isUserInteracting = true);
              },
              child: LayoutBuilder(
                builder: (ctx, constraints) {
                  final sx = constraints.maxWidth / _mapW;
                  final sy = constraints.maxHeight / _mapH;
                  
                  return Stack(
                    fit: StackFit.expand,
                    children: [
                      Image.asset(
                        asset,
                        fit: BoxFit.fill,
                        gaplessPlayback: true,
                      ),
                      if (widget.coord != null)
                        AnimatedPositioned(
                          duration: const Duration(milliseconds: 400),
                          curve: Curves.easeInOutCubic,
                          left: widget.coord!.x * sx - 15,
                          top: widget.coord!.y * sy - 15,
                          child: const _PulsingMarker(),
                        ),
                    ],
                  );
                },
              ),
            ),
          ),
        ),
        const Padding(
          padding: EdgeInsets.only(top: 6, left: 4),
          child: Text(
            'Tip: Pinch to zoom. "Center" snaps back to your location.',
            style: TextStyle(fontSize: 10, color: Colors.grey),
          ),
        ),
      ],
    );
  }
}

class _PulsingMarker extends StatefulWidget {
  const _PulsingMarker();

  @override
  State<_PulsingMarker> createState() => _PulsingMarkerState();
}

class _PulsingMarkerState extends State<_PulsingMarker> with SingleTickerProviderStateMixin {
  late final AnimationController _ctrl;

  @override
  void initState() {
    super.initState();
    _ctrl = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 1500),
    )..repeat();
  }

  @override
  void dispose() {
    _ctrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: 30,
      height: 30,
      child: Stack(
        alignment: Alignment.center,
        children: [
          AnimatedBuilder(
            animation: _ctrl,
            builder: (_, __) {
              final t = _ctrl.value;
              return Container(
                width: 15 + 25 * t,
                height: 15 + 25 * t,
                decoration: BoxDecoration(
                  shape: BoxShape.circle,
                  color: Colors.red.withOpacity((1 - t) * 0.4),
                ),
              );
            },
          ),
          Container(
            width: 14,
            height: 14,
            decoration: BoxDecoration(
              color: Colors.red,
              shape: BoxShape.circle,
              border: Border.all(color: Colors.white, width: 2),
              boxShadow: const [
                BoxShadow(color: Colors.black26, blurRadius: 4, offset: Offset(0, 2)),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
