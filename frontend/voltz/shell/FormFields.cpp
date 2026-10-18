#include "FormFields.hpp"

#include <QEvent>
#include <QObject>
#include <QWidget>

namespace voltz {

namespace {

// A roda sem o foco não chega ao campo (o wheelEvent do QSlider/QComboBox mudaria o valor) e segue para os pais: o
// filtro devolve true (o campo não vê o evento) com o evento IGNORADO. Para a roda espontânea, o QApplication::notify
// (qapplication.cpp, case QEvent::Wheel) entrega o mesmo QWheelEvent ao widget sob o mouse e, enquanto
// `!(res && we.isAccepted())`, sobe pelo parentWidget() (até uma janela ou WA_NoMousePropagation), com a posição
// convertida; o filtro faz parte do notify_helper desse widget. A área de rolagem da página (viewport → QScrollArea →
// barra vertical) aceita e rola, como sobre qualquer rótulo da página.
class WheelGuard : public QObject {
public:
	using QObject::QObject;

protected:
	bool eventFilter(QObject *watched, QEvent *event) override
	{
		if (event->type() == QEvent::Wheel && watched->isWidgetType() &&
		    !static_cast<QWidget *>(watched)->hasFocus()) {
			event->ignore();
			return true;
		}
		return QObject::eventFilter(watched, event);
	}
};

} // namespace

void guardWheel(QWidget *field)
{
	field->setFocusPolicy(Qt::StrongFocus);
	field->installEventFilter(new WheelGuard(field));
}

} // namespace voltz
