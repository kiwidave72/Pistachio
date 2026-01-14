#include "SketchDocumentMapper.h"

#include <stdexcept>

namespace adapters::persistence {

    using namespace domain::sketch;

    static dto::EntityRefDto toDtoRef(const EntityRef& r) {
        return dto::EntityRefDto{ r.id, r.anchor };
    }

    static EntityRef fromDtoRef(const dto::EntityRefDto& r) {
        return EntityRef{ r.id, r.anchor };
    }

    static dto::ConstraintMetaDto toDtoMeta(const ConstraintMeta& m) {
        dto::ConstraintMetaDto out;
        out.id = m.id;
        out.name = m.name;
        out.enabled = m.enabled;
        out.suppressed = m.suppressed;
        return out;
    }

    static ConstraintMeta fromDtoMeta(const dto::ConstraintMetaDto& m) {
        ConstraintMeta out;
        out.id = m.id;
        out.name = m.name;
        out.enabled = m.enabled;
        out.suppressed = m.suppressed;
        return out;
    }

    dto::FileDto SketchDocumentMapper::toDto(const Document& doc) {
        dto::FileDto file;
        file.fileVersion = dto::kSketchFileVersion;

        dto::DocumentDto d;
        d.id = doc.id;
        d.name = doc.name;

        d.sketches.reserve(doc.sketches.size());
        for (const auto& s : doc.sketches) {
            dto::SketchDto sd;
            sd.id = s.id;
            sd.name = s.name;
            sd.visible = s.visible;

            // Entities - flatten typed vectors into a single list.
            for (const auto& p : s.entities.points()) {
                dto::PointDto e;
                e.id = p.h.id;
                e.name = p.h.name;
                e.construction = p.h.construction;
                e.visible = p.h.visible;
                e.selectable = p.h.selectable;
                e.p = p.p;
                sd.entities.emplace_back(e);
            }
            for (const auto& l : s.entities.lines()) {
                dto::LineDto e;
                e.id = l.h.id;
                e.name = l.h.name;
                e.construction = l.h.construction;
                e.visible = l.h.visible;
                e.selectable = l.h.selectable;
                e.a = l.a;
                e.b = l.b;
                sd.entities.emplace_back(e);
            }
            for (const auto& c : s.entities.circles()) {
                dto::CircleDto e;
                e.id = c.h.id;
                e.name = c.h.name;
                e.construction = c.h.construction;
                e.visible = c.h.visible;
                e.selectable = c.h.selectable;
                e.center = c.center;
                e.radius = c.radius;
                sd.entities.emplace_back(e);
            }
            for (const auto& a : s.entities.arcs()) {
                dto::ArcDto e;
                e.id = a.h.id;
                e.name = a.h.name;
                e.construction = a.h.construction;
                e.visible = a.h.visible;
                e.selectable = a.h.selectable;
                e.center = a.center;
                e.radius = a.radius;
                e.start = a.start;
                e.end = a.end;
                e.ccw = a.ccw;
                sd.entities.emplace_back(e);
            }
            for (const auto& e0 : s.entities.ellipses()) {
                dto::EllipseDto e;
                e.id = e0.h.id;
                e.name = e0.h.name;
                e.construction = e0.h.construction;
                e.visible = e0.h.visible;
                e.selectable = e0.h.selectable;
                e.center = e0.center;
                e.rx = e0.rx;
                e.ry = e0.ry;
                e.rotation = e0.rotation;
                sd.entities.emplace_back(e);
            }
            for (const auto& cv : s.entities.curves()) {
                dto::CurveDto e;
                e.id = cv.h.id;
                e.name = cv.h.name;
                e.construction = cv.h.construction;
                e.visible = cv.h.visible;
                e.selectable = cv.h.selectable;
                e.controlPoints = cv.controlPoints;
                e.closed = cv.closed;
                sd.entities.emplace_back(e);
            }

            // Constraints
            sd.constraints.reserve(s.constraints.size());
            for (const auto& c : s.constraints) {
                if (std::holds_alternative<GeometricConstraint>(c)) {
                    const auto& gc = std::get<GeometricConstraint>(c);
                    dto::GeometricConstraintDto out;
                    out.meta = toDtoMeta(gc.meta);
                    out.type = gc.type;
                    out.param = gc.param;
                    out.refs.reserve(gc.refs.size());
                    for (const auto& r : gc.refs) out.refs.push_back(toDtoRef(r));
                    sd.constraints.emplace_back(out);
                } else {
                    const auto& dc = std::get<DimensionalConstraint>(c);
                    dto::DimensionalConstraintDto out;
                    out.meta = toDtoMeta(dc.meta);
                    out.type = dc.type;
                    out.value = dc.value;
                    out.driving = dc.driving;
                    out.units = dc.units;
                    out.refs.reserve(dc.refs.size());
                    for (const auto& r : dc.refs) out.refs.push_back(toDtoRef(r));
                    sd.constraints.emplace_back(out);
                }
            }

            d.sketches.emplace_back(std::move(sd));
        }

        file.document = std::move(d);
        return file;
    }

    std::shared_ptr<Document> SketchDocumentMapper::fromDto(const dto::FileDto& file) {
        if (file.fileVersion != dto::kSketchFileVersion) {
            // For now: hard fail. Later: migrate older versions here.
            throw std::runtime_error("Unsupported sketch fileVersion: " + std::to_string(file.fileVersion));
        }

        auto doc = std::make_shared<Document>();
        doc->id = file.document.id;
        doc->name = file.document.name;

        doc->sketches.reserve(file.document.sketches.size());
        for (const auto& s : file.document.sketches) {
            Sketch sk;
            sk.id = s.id;
            sk.name = s.name;
            sk.visible = s.visible;

            // Entities
            for (const auto& ent : s.entities) {
                std::visit([&](auto&& e) {
                    using T = std::decay_t<decltype(e)>;
                    if constexpr (std::is_same_v<T, dto::PointDto>) {
                        Point2D p;
                        p.h.id = e.id;
                        p.h.name = e.name;
                        p.h.construction = e.construction;
                        p.h.visible = e.visible;
                        p.h.selectable = e.selectable;
                        p.p = e.p;
                        sk.entities.addPoint(std::move(p));
                    } else if constexpr (std::is_same_v<T, dto::LineDto>) {
                        Line2D l;
                        l.h.id = e.id;
                        l.h.name = e.name;
                        l.h.construction = e.construction;
                        l.h.visible = e.visible;
                        l.h.selectable = e.selectable;
                        l.a = e.a;
                        l.b = e.b;
                        sk.entities.addLine(std::move(l));
                    } else if constexpr (std::is_same_v<T, dto::CircleDto>) {
                        Circle2D c;
                        c.h.id = e.id;
                        c.h.name = e.name;
                        c.h.construction = e.construction;
                        c.h.visible = e.visible;
                        c.h.selectable = e.selectable;
                        c.center = e.center;
                        c.radius = e.radius;
                        sk.entities.addCircle(std::move(c));
                    } else if constexpr (std::is_same_v<T, dto::ArcDto>) {
                        Arc2D a;
                        a.h.id = e.id;
                        a.h.name = e.name;
                        a.h.construction = e.construction;
                        a.h.visible = e.visible;
                        a.h.selectable = e.selectable;
                        a.center = e.center;
                        a.radius = e.radius;
                        a.start = e.start;
                        a.end = e.end;
                        a.ccw = e.ccw;
                        sk.entities.addArc(std::move(a));
                    } else if constexpr (std::is_same_v<T, dto::EllipseDto>) {
                        Ellipse2D el;
                        el.h.id = e.id;
                        el.h.name = e.name;
                        el.h.construction = e.construction;
                        el.h.visible = e.visible;
                        el.h.selectable = e.selectable;
                        el.center = e.center;
                        el.rx = e.rx;
                        el.ry = e.ry;
                        el.rotation = e.rotation;
                        sk.entities.addEllipse(std::move(el));
                    } else if constexpr (std::is_same_v<T, dto::CurveDto>) {
                        Curve2D cv;
                        cv.h.id = e.id;
                        cv.h.name = e.name;
                        cv.h.construction = e.construction;
                        cv.h.visible = e.visible;
                        cv.h.selectable = e.selectable;
                        cv.controlPoints = e.controlPoints;
                        cv.closed = e.closed;
                        sk.entities.addCurve(std::move(cv));
                    }
                }, ent);
            }

            // Constraints
            sk.constraints.reserve(s.constraints.size());
            for (const auto& c : s.constraints) {
                std::visit([&](auto&& cc) {
                    using T = std::decay_t<decltype(cc)>;
                    if constexpr (std::is_same_v<T, dto::GeometricConstraintDto>) {
                        GeometricConstraint gc;
                        gc.meta = fromDtoMeta(cc.meta);
                        gc.type = cc.type;
                        gc.param = cc.param;
                        gc.refs.reserve(cc.refs.size());
                        for (const auto& r : cc.refs) gc.refs.push_back(fromDtoRef(r));
                        sk.constraints.emplace_back(std::move(gc));
                    } else if constexpr (std::is_same_v<T, dto::DimensionalConstraintDto>) {
                        DimensionalConstraint dc;
                        dc.meta = fromDtoMeta(cc.meta);
                        dc.type = cc.type;
                        dc.value = cc.value;
                        dc.driving = cc.driving;
                        dc.units = cc.units;
                        dc.refs.reserve(cc.refs.size());
                        for (const auto& r : cc.refs) dc.refs.push_back(fromDtoRef(r));
                        sk.constraints.emplace_back(std::move(dc));
                    }
                }, c);
            }

            doc->sketches.emplace_back(std::move(sk));
        }

        return doc;
    }

} // namespace adapters::persistence
